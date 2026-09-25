#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace chat_ai::test {

// 将验证失败的具体原因交给上层处理，便于后续映射成统一的 HTTP 响应。
enum class VerificationResult {
    Success,
    NotFound,
    Expired,
    Incorrect,
    TooManyAttempts,
    AlreadyUsed
};

// 告诉调用方本次是否真正生成了验证码。
enum class GenerationStatus {
    Success,
    TooFrequent
};

struct GenerationResult {
    GenerationStatus status = GenerationStatus::Success;
    std::string code;
    // 请求过于频繁时，提示距离下次允许生成还剩多少秒。
    std::chrono::seconds retry_after{0};
};

class VerificationCodeManager {
public:
    // 每个管理器启动时生成独立的摘要密钥；同一邮箱默认一分钟只能生成一次。
    explicit VerificationCodeManager(
        std::chrono::seconds resend_interval = std::chrono::minutes(1)
    );

    // 为指定邮箱和用途生成六位数字验证码。再次生成会使旧验证码失效。
    GenerationResult GenerateCode(
        const std::string& email,
        const std::string& purpose,
        std::chrono::seconds valid_for = std::chrono::minutes(5),  // TODO
        std::size_t max_attempts = 5
    );

    // 验证码与邮箱、用途绑定；验证成功后不可再次使用。
    VerificationResult VerifyCode(
        const std::string& email,
        const std::string& purpose,
        const std::string& code
    );

private:
    // SHA-256 摘要和服务密钥均使用 32 字节。
    static constexpr std::size_t kSecretSize = 32;
    static constexpr std::size_t kDigestSize = 32;

    // 测试阶段使用内存记录；后续接入 DataManager 时可将这些字段持久化。
    struct CodeRecord {
        std::array<unsigned char, kDigestSize> digest{};
        std::chrono::steady_clock::time_point expires_at;
        std::size_t attempts_remaining = 0;
        bool consumed = false;
    };

    std::string GenerateRandomDigits(std::size_t length) const;
    std::array<unsigned char, kDigestSize> ComputeDigest(
        const std::string& email,
        const std::string& purpose,
        const std::string& code
    ) const;
    static std::string MakeRecordKey(
        const std::string& email,
        const std::string& purpose
    );

    std::array<unsigned char, kSecretSize> secret_{};
    // 冷却时间按邮箱计算，与验证码的业务用途无关。
    std::chrono::seconds resend_interval_;
    std::unordered_map<
        std::string,
        std::chrono::steady_clock::time_point
    > resend_available_at_;
    // key 由 email 和 purpose 共同组成，保证不同业务的验证码相互隔离。
    std::unordered_map<std::string, CodeRecord> records_;
    // 保护验证码记录，允许多个请求线程安全地生成和验证验证码。
    std::mutex mutex_;
};

} // namespace chat_ai::test
