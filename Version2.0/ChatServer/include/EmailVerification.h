#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace ai_chat_server {

// SendCode 的结果由 HTTP 层转换成合适的状态码。
enum class SendCodeStatus {
    Success,
    TooFrequent,
    NotConfigured,
    InvalidArgument,
    DeliveryFailed,
    InternalError
};

struct SendCodeResult {
    SendCodeStatus status = SendCodeStatus::InternalError;
    // 触发发送频率限制时，表示还需要等待多少秒。
    std::chrono::seconds retry_after{0};
    // 仅用于服务器日志，不应原样返回给客户端。
    std::string detail;
};

// VerifyCode 的结果由 HTTP 层转换成统一响应。
enum class VerifyCodeResult {
    Success,
    NotFound,
    Expired,
    Incorrect,
    TooManyAttempts,
    AlreadyUsed,
    InvalidArgument,
    InternalError
};

class EmailVerificationService {
public:
    // 默认从 QQ_SMTP_USER 和 QQ_SMTP_AUTH_CODE 环境变量读取 SMTP 凭据。
    explicit EmailVerificationService(
        std::chrono::seconds resend_interval = std::chrono::minutes(1),
        std::chrono::seconds code_validity = std::chrono::minutes(5),
        std::size_t max_attempts = 5
    );

    // 生成六位验证码并通过 QQ 邮箱发送。响应中不会暴露验证码明文。
    SendCodeResult SendCode(
        const std::string& email,
        const std::string& purpose
    );

    // 验证码与邮箱及用途绑定；验证成功后不能重复使用。
    VerifyCodeResult VerifyCode(
        const std::string& email,
        const std::string& purpose,
        const std::string& code
    );

    // 可用于服务器启动日志或健康检查，不会返回具体授权码。
    bool IsConfigured() const noexcept;

private:
    static constexpr std::size_t kSecretSize = 32;
    static constexpr std::size_t kDigestSize = 32;

    struct CodeRecord {
        std::array<unsigned char, kDigestSize> digest{};
        std::chrono::steady_clock::time_point expires_at;
        std::size_t attempts_remaining = 0;
        std::uint64_t generation = 0;
        bool consumed = false;
    };

    struct CooldownRecord {
        std::chrono::steady_clock::time_point available_at;
        std::uint64_t generation = 0;
    };

    struct PendingCode {
        SendCodeStatus status = SendCodeStatus::InternalError;
        std::string code;
        std::uint64_t generation = 0;
        std::chrono::seconds retry_after{0};
        std::string detail;
    };

    PendingCode GenerateCode(
        const std::string& email,
        const std::string& purpose
    );
    void RollbackGeneration(
        const std::string& email,
        const std::string& purpose,
        std::uint64_t generation
    );
    bool SendQQEmail(
        const std::string& recipient,
        const std::string& code,
        std::string* detail
    ) const;
    std::string GenerateRandomDigits(std::size_t length) const;
    std::array<unsigned char, kDigestSize> ComputeDigest(
        const std::string& email,
        const std::string& purpose,
        const std::string& code
    ) const;

    static bool IsValidEmail(const std::string& email);
    static bool IsValidPurpose(const std::string& purpose);
    static bool IsSixDigitCode(const std::string& code);
    static std::string MakeRecordKey(
        const std::string& email,
        const std::string& purpose
    );

    std::array<unsigned char, kSecretSize> secret_{};
    std::chrono::seconds resend_interval_;
    std::chrono::seconds code_validity_;
    std::size_t max_attempts_;
    std::string smtp_user_;
    std::string smtp_auth_code_;
    std::unordered_map<std::string, CodeRecord> records_;
    std::unordered_map<std::string, CooldownRecord> cooldowns_;
    std::uint64_t next_generation_ = 0;
    std::mutex mutex_;
};

} // namespace ai_chat_server
