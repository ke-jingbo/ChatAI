#include "../include/EmailVerification.h"

#include <curl/curl.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace ai_chat_server {
namespace {

struct UploadContext {
    const std::string* content = nullptr;
    std::size_t offset = 0;
};

std::size_t ReadEmailData(char* buffer, std::size_t size,
                          std::size_t count, void* user_data) {
    auto* context = static_cast<UploadContext*>(user_data);
    const std::size_t capacity = size * count;
    if(context == nullptr || context->content == nullptr || capacity == 0 ||
       context->offset >= context->content->size()) {
        return 0;
    }

    const std::size_t remaining = context->content->size() - context->offset;
    const std::size_t copy_size = std::min(capacity, remaining);
    std::memcpy(
        buffer,
        context->content->data() + context->offset,
        copy_size
    );
    context->offset += copy_size;
    return copy_size;
}

std::string BuildVerificationEmail(const std::string& sender,
                                   const std::string& recipient,
                                   const std::string& code,
                                   std::chrono::seconds validity) {
    // 邮件头与正文之间必须有空行，并使用 SMTP 要求的 CRLF 换行。
    return
        "From: ChatAI <" + sender + ">\r\n"
        "To: <" + recipient + ">\r\n"
        "Subject: ChatAI verification code\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: text/plain; charset=UTF-8\r\n"
        "Content-Transfer-Encoding: 8bit\r\n"
        "\r\n"
        "Your ChatAI verification code is: [ " + code + " ]\r\n"
        "The code is valid for " + std::to_string(validity.count()) +
        " seconds.\r\n"
        "If you did not request this code, please ignore this email.\r\n";
}

// 函数内静态对象的初始化由 C++ 保证线程安全，避免多个 HTTP 请求重复
// 调用 curl_global_init。进程退出时析构函数负责执行全局清理。
class CurlGlobalState {
public:
    CurlGlobalState() : result_(curl_global_init(CURL_GLOBAL_DEFAULT)) {}
    ~CurlGlobalState() {
        if(result_ == CURLE_OK) curl_global_cleanup();
    }

    CURLcode result() const noexcept { return result_; }

private:
    CURLcode result_;
};

CURLcode EnsureCurlInitialized() {
    static CurlGlobalState state;
    return state.result();
}

} // namespace

EmailVerificationService::EmailVerificationService(
    std::chrono::seconds resend_interval,
    std::chrono::seconds code_validity,
    std::size_t max_attempts
) : resend_interval_(resend_interval),
    code_validity_(code_validity),
    max_attempts_(max_attempts) {
    if(resend_interval_ <= std::chrono::seconds::zero()) {
        throw std::invalid_argument("验证码重发间隔必须大于零");
    }
    if(code_validity_ <= std::chrono::seconds::zero()) {
        throw std::invalid_argument("验证码有效期必须大于零");
    }
    if(max_attempts_ == 0) {
        throw std::invalid_argument("验证码最大尝试次数必须大于零");
    }

    // HMAC 密钥只保存在当前服务器进程中，数据库或内存记录泄露时也不能
    // 直接对六位验证码进行无密钥比对。
    if(RAND_bytes(secret_.data(), static_cast<int>(secret_.size())) != 1) {
        throw std::runtime_error("生成验证码服务密钥失败");
    }

    // SMTP 授权码属于服务器秘密，只从环境变量读取，绝不写入源码或日志。
    const char* smtp_user = std::getenv("QQ_SMTP_USER");
    const char* smtp_auth_code = std::getenv("QQ_SMTP_AUTH_CODE");
    if(smtp_user != nullptr) smtp_user_ = smtp_user;
    if(smtp_auth_code != nullptr) smtp_auth_code_ = smtp_auth_code;
}

SendCodeResult EmailVerificationService::SendCode(
    const std::string& email,
    const std::string& purpose
) {
    if(!IsValidEmail(email) || !IsValidPurpose(purpose)) {
        return {
            SendCodeStatus::InvalidArgument,
            std::chrono::seconds::zero(),
            "邮箱或验证码用途格式无效"
        };
    }
    if(!IsConfigured()) {
        return {
            SendCodeStatus::NotConfigured,
            std::chrono::seconds::zero(),
            "未设置 QQ_SMTP_USER 或 QQ_SMTP_AUTH_CODE"
        };
    }

    PendingCode pending;
    try {
        pending = GenerateCode(email, purpose);
    } catch(const std::exception& error) {
        return {
            SendCodeStatus::InternalError,
            std::chrono::seconds::zero(),
            error.what()
        };
    }

    if(pending.status != SendCodeStatus::Success) {
        return {pending.status, pending.retry_after, pending.detail};
    }

    std::string detail;
    if(!SendQQEmail(email, pending.code, &detail)) {
        // 邮件没有送出时，用户不可能知道验证码。只回滚当前这一代记录，
        // 避免耗尽冷却时间，同时防止误删并发产生的新验证码。
        RollbackGeneration(email, purpose, pending.generation);
        return {
            SendCodeStatus::DeliveryFailed,
            std::chrono::seconds::zero(),
            std::move(detail)
        };
    }

    return {
        SendCodeStatus::Success,
        std::chrono::seconds::zero(),
        ""
    };
}

VerifyCodeResult EmailVerificationService::VerifyCode(
    const std::string& email,
    const std::string& purpose,
    const std::string& code
) {
    if(!IsValidEmail(email) || !IsValidPurpose(purpose) ||
       !IsSixDigitCode(code)) {
        return VerifyCodeResult::InvalidArgument;
    }

    try {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto iterator = records_.find(MakeRecordKey(email, purpose));
        if(iterator == records_.end()) return VerifyCodeResult::NotFound;

        CodeRecord& record = iterator->second;
        if(record.consumed) return VerifyCodeResult::AlreadyUsed;
        if(std::chrono::steady_clock::now() >= record.expires_at) {
            return VerifyCodeResult::Expired;
        }
        if(record.attempts_remaining == 0) {
            return VerifyCodeResult::TooManyAttempts;
        }

        const auto candidate = ComputeDigest(email, purpose, code);
        // 恒定时间比较可避免普通字符串比较带来的时序差异。
        if(CRYPTO_memcmp(
               record.digest.data(),
               candidate.data(),
               record.digest.size()
           ) != 0) {
            --record.attempts_remaining;
            return record.attempts_remaining == 0
                ? VerifyCodeResult::TooManyAttempts
                : VerifyCodeResult::Incorrect;
        }

        record.consumed = true;
        return VerifyCodeResult::Success;
    } catch(...) {
        return VerifyCodeResult::InternalError;
    }
}

bool EmailVerificationService::IsConfigured() const noexcept {
    return !smtp_user_.empty() && !smtp_auth_code_.empty() &&
           IsValidEmail(smtp_user_);
}

EmailVerificationService::PendingCode EmailVerificationService::GenerateCode(
    const std::string& email,
    const std::string& purpose
) {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);

    const auto cooldown = cooldowns_.find(email);
    if(cooldown != cooldowns_.end() && now < cooldown->second.available_at) {
        return {
            SendCodeStatus::TooFrequent,
            "",
            0,
            std::chrono::ceil<std::chrono::seconds>(
                cooldown->second.available_at - now
            ),
            "验证码发送过于频繁"
        };
    }

    const std::string code = GenerateRandomDigits(6);
    // generation 只在锁内递增，用于 SMTP 失败时精确回滚这一代记录。
    const std::uint64_t generation = ++next_generation_;

    CodeRecord record;
    record.digest = ComputeDigest(email, purpose, code);
    record.expires_at = now + code_validity_;
    record.attempts_remaining = max_attempts_;
    record.generation = generation;
    records_[MakeRecordKey(email, purpose)] = record;
    cooldowns_[email] = {now + resend_interval_, generation};

    return {
        SendCodeStatus::Success,
        code,
        generation,
        std::chrono::seconds::zero(),
        ""
    };
}

void EmailVerificationService::RollbackGeneration(
    const std::string& email,
    const std::string& purpose,
    std::uint64_t generation
) {
    std::lock_guard<std::mutex> lock(mutex_);

    const std::string key = MakeRecordKey(email, purpose);
    const auto record = records_.find(key);
    if(record != records_.end() && record->second.generation == generation) {
        records_.erase(record);
    }

    const auto cooldown = cooldowns_.find(email);
    if(cooldown != cooldowns_.end() &&
       cooldown->second.generation == generation) {
        cooldowns_.erase(cooldown);
    }
}

bool EmailVerificationService::SendQQEmail(
    const std::string& recipient,
    const std::string& code,
    std::string* detail
) const {
    const CURLcode global_result = EnsureCurlInitialized();
    if(global_result != CURLE_OK) {
        if(detail != nullptr) {
            *detail = std::string("初始化 libcurl 失败：") +
                      curl_easy_strerror(global_result);
        }
        return false;
    }

    CURL* curl = curl_easy_init();
    if(curl == nullptr) {
        if(detail != nullptr) *detail = "创建 CURL SMTP 请求失败";
        return false;
    }

    const std::string mail_from = "<" + smtp_user_ + ">";
    const std::string mail_to = "<" + recipient + ">";
    const std::string email = BuildVerificationEmail(
        smtp_user_,
        recipient,
        code,
        code_validity_
    );
    UploadContext upload_context{&email, 0};
    curl_slist* recipients = curl_slist_append(nullptr, mail_to.c_str());
    if(recipients == nullptr) {
        if(detail != nullptr) *detail = "创建 SMTP 收件人列表失败";
        curl_easy_cleanup(curl);
        return false;
    }

    char error_buffer[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_URL, "smtps://smtp.qq.com:465");
    curl_easy_setopt(curl, CURLOPT_USERNAME, smtp_user_.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, smtp_auth_code_.c_str());
    // QQ SMTP 在当前环境中需要明确选择 LOGIN 认证方式。
    curl_easy_setopt(curl, CURLOPT_LOGIN_OPTIONS, "AUTH=LOGIN");
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, mail_from.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, ReadEmailData);
    curl_easy_setopt(curl, CURLOPT_READDATA, &upload_context);
    curl_easy_setopt(
        curl,
        CURLOPT_INFILESIZE_LARGE,
        static_cast<curl_off_t>(email.size())
    );
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    // 始终校验 SMTP 服务器证书，不能通过关闭 TLS 校验解决连接问题。
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    // 多线程服务器中禁用信号，避免超时处理干扰其他请求线程。
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);

    const CURLcode result = curl_easy_perform(curl);
    if(result != CURLE_OK && detail != nullptr) {
        *detail = error_buffer[0] != '\0'
            ? error_buffer
            : curl_easy_strerror(result);
    }

    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);
    return result == CURLE_OK;
}

std::string EmailVerificationService::GenerateRandomDigits(
    std::size_t length
) const {
    std::string code;
    code.reserve(length);

    while(code.size() < length) {
        unsigned char value = 0;
        if(RAND_bytes(&value, 1) != 1) {
            throw std::runtime_error("生成随机验证码失败");
        }
        // 只接受 0～249，避免 256 对 10 取模产生分布偏差。
        if(value < 250) {
            code.push_back(static_cast<char>('0' + value % 10));
        }
    }
    return code;
}

std::array<unsigned char, EmailVerificationService::kDigestSize>
EmailVerificationService::ComputeDigest(
    const std::string& email,
    const std::string& purpose,
    const std::string& code
) const {
    // 字段长度消除了直接拼接可能出现的边界歧义。
    const std::string input =
        std::to_string(email.size()) + ":" + email +
        std::to_string(purpose.size()) + ":" + purpose + code;

    std::array<unsigned char, kDigestSize> digest{};
    unsigned int digest_length = 0;
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

bool EmailVerificationService::IsValidEmail(const std::string& email) {
    if(email.empty() || email.size() > 254) return false;
    if(std::any_of(email.begin(), email.end(), [](unsigned char character) {
           return std::iscntrl(character) != 0 ||
                  std::isspace(character) != 0;
       })) {
        return false;
    }

    const std::size_t at = email.find('@');
    return at != std::string::npos && at > 0 && at + 1 < email.size() &&
           email.find('@', at + 1) == std::string::npos;
}

bool EmailVerificationService::IsValidPurpose(const std::string& purpose) {
    if(purpose.empty() || purpose.size() > 64) return false;
    return std::all_of(
        purpose.begin(),
        purpose.end(),
        [](unsigned char character) {
            return std::isalnum(character) != 0 || character == '_' ||
                   character == '-';
        }
    );
}

bool EmailVerificationService::IsSixDigitCode(const std::string& code) {
    return code.size() == 6 &&
           std::all_of(code.begin(), code.end(), [](unsigned char character) {
               return std::isdigit(character) != 0;
           });
}

std::string EmailVerificationService::MakeRecordKey(
    const std::string& email,
    const std::string& purpose
) {
    return std::to_string(email.size()) + ":" + email + purpose;
}

} // namespace ai_chat_server
