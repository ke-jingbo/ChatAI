#include "VerificationCode.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <stdexcept>

namespace chat_ai::test {

VerificationCodeManager::VerificationCodeManager(
    std::chrono::seconds resend_interval
) : resend_interval_(resend_interval) {
    // 1. 冷却时间必须为正数，避免配置为零后失去频率限制。
    if(resend_interval_ <= std::chrono::seconds::zero()) {
        throw std::invalid_argument("验证码重新生成间隔必须大于零");
    }

    // 2. 使用操作系统安全随机源生成本进程的 HMAC 密钥。
    if(RAND_bytes(secret_.data(), static_cast<int>(secret_.size())) != 1) {
        throw std::runtime_error("生成验证码服务密钥失败");
    }
}

GenerationResult VerificationCodeManager::GenerateCode(
    const std::string& email,
    const std::string& purpose,
    std::chrono::seconds valid_for,
    std::size_t max_attempts
) {
    // 1. 先拒绝无法形成有效验证码记录的参数。
    if(email.empty() || purpose.empty()) {
        throw std::invalid_argument("邮箱和验证码用途不能为空");
    }
    if(valid_for <= std::chrono::seconds::zero()) {
        throw std::invalid_argument("验证码有效期必须大于零");
    }
    if(max_attempts == 0) {
        throw std::invalid_argument("验证码最大尝试次数必须大于零");
    }

    const auto now = std::chrono::steady_clock::now();

    // 2. 限流检查和冷却时间写入必须处于同一临界区，避免并发请求同时成功。
    std::lock_guard<std::mutex> lock(mutex_);
    const auto resend_iterator = resend_available_at_.find(email);
    if(resend_iterator != resend_available_at_.end() &&
       now < resend_iterator->second) {
        // 向上取整，避免还有不足一秒时向调用方返回 0 秒。
        const auto retry_after = std::chrono::ceil<std::chrono::seconds>(
            resend_iterator->second - now
        );
        return {GenerationStatus::TooFrequent, "", retry_after};
    }

    // 3. 通过限流检查后，生成只用于发送给用户的六位明文验证码。
    const std::string code = GenerateRandomDigits(6);

    // 4. 记录中只保存验证码摘要、过期时间和剩余尝试次数。
    CodeRecord record;
    record.digest = ComputeDigest(email, purpose, code);
    record.expires_at = now + valid_for;
    record.attempts_remaining = max_attempts;

    // 5. 保存新记录，并从本次成功生成时间开始计算一分钟冷却期。
    records_[MakeRecordKey(email, purpose)] = record;
    resend_available_at_[email] = now + resend_interval_;
    return {GenerationStatus::Success, code, std::chrono::seconds::zero()};
}

VerificationResult VerificationCodeManager::VerifyCode(
    const std::string& email,
    const std::string& purpose,
    const std::string& code
) {
    // 1. 验证过程需要原子地检查并更新次数或消费状态，因此全程持锁。
    std::lock_guard<std::mutex> lock(mutex_);

    // 2. 只查找与当前邮箱及业务用途完全匹配的验证码记录。
    const auto iterator = records_.find(MakeRecordKey(email, purpose));
    if(iterator == records_.end()) {
        return VerificationResult::NotFound;
    }

    CodeRecord& record = iterator->second;

    // 3. 按状态优先级依次拒绝已使用、已过期和已耗尽次数的记录。
    if(record.consumed) {
        return VerificationResult::AlreadyUsed;
    }
    if(std::chrono::steady_clock::now() >= record.expires_at) {
        return VerificationResult::Expired;
    }
    if(record.attempts_remaining == 0) {
        return VerificationResult::TooManyAttempts;
    }

    // 4. 对用户输入执行同样的 HMAC 运算，不需要恢复或读取明文验证码。
    const auto candidate_digest = ComputeDigest(email, purpose, code);

    // 5. 使用恒定时间比较，避免普通字符串比较带来的时序差异。
    if(CRYPTO_memcmp(
           record.digest.data(),
           candidate_digest.data(),
           record.digest.size()
       ) != 0) {
        // 6. 每次错误都会扣减次数；本次耗尽时直接返回次数超限。
        --record.attempts_remaining;
        return record.attempts_remaining == 0
            ? VerificationResult::TooManyAttempts
            : VerificationResult::Incorrect;
    }

    // 7. 正确验证码只允许消费一次，后续重复提交返回 AlreadyUsed。
    record.consumed = true;
    return VerificationResult::Success;
}

std::string VerificationCodeManager::GenerateRandomDigits(
    std::size_t length
) const {
    std::string code;
    code.reserve(length);

    // 持续读取安全随机字节，直到收集到指定数量的十进制数字。
    while(code.size() < length) {
        unsigned char value = 0;
        if(RAND_bytes(&value, 1) != 1) {
            throw std::runtime_error("生成验证码失败");
        }

        // 只接受 0～249，避免直接对 256 取模造成数字分布偏差。
        if(value < 250) {
            code.push_back(static_cast<char>('0' + value % 10));
        }
    }

    return code;
}

std::array<unsigned char, VerificationCodeManager::kDigestSize>
VerificationCodeManager::ComputeDigest(
    const std::string& email,
    const std::string& purpose,
    const std::string& code
) const {
    // 加入字段长度，避免不同 email、purpose 和 code 组合产生相同拼接内容。
    const std::string input =
        std::to_string(email.size()) + ":" + email +
        std::to_string(purpose.size()) + ":" + purpose + code;

    std::array<unsigned char, kDigestSize> digest{};
    unsigned int digest_length = 0;

    // 使用服务端秘密作为密钥计算 HMAC-SHA256，数据库泄露时也不能直接比对明文。
    unsigned char* result = HMAC(
        EVP_sha256(),
        secret_.data(),
        static_cast<int>(secret_.size()),
        reinterpret_cast<const unsigned char*>(input.data()),
        input.size(),
        digest.data(),
        &digest_length
    );

    if(result == nullptr || digest_length != digest.size()) {
        throw std::runtime_error("计算验证码摘要失败");
    }
    return digest;
}

std::string VerificationCodeManager::MakeRecordKey(
    const std::string& email,
    const std::string& purpose
) {
    // email 长度用于消除字符串直接拼接时的边界歧义。
    return std::to_string(email.size()) + ":" + email + purpose;
}

} // namespace chat_ai::test
