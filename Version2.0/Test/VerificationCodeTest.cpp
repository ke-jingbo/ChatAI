#include "VerificationCode.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

using chat_ai::test::VerificationCodeManager;
using chat_ai::test::VerificationResult;
using chat_ai::test::GenerationStatus;

bool Expect(bool condition, const std::string& message) {
    // 用统一出口打印失败原因，让独立测试程序不依赖额外测试框架。
    if(!condition) {
        std::cerr << "测试失败：" << message << '\n';
        return false;
    }
    return true;
}

bool TestGeneratedCodeFormat() {
    // 验证生成结果固定为六位数字，包含前导零时也不会丢失长度。
    VerificationCodeManager manager;
    const auto result = manager.GenerateCode("user@example.com", "register");
    return Expect(result.status == GenerationStatus::Success, "首次生成应成功") &&
           Expect(result.code.size() == 6, "验证码长度应为 6") &&
           Expect(
               std::all_of(result.code.begin(), result.code.end(), [](unsigned char character) {
                   return std::isdigit(character) != 0;
               }),
               "验证码应全部由数字组成"
           );
}

bool TestSuccessfulVerificationIsOneTime() {
    // 先验证正确输入，再验证同一个验证码不能被重复消费。
    VerificationCodeManager manager;
    const auto generation = manager.GenerateCode("user@example.com", "register");
    return Expect(
               generation.status == GenerationStatus::Success,
               "首次生成应成功"
           ) &&
           Expect(
               manager.VerifyCode("user@example.com", "register", generation.code) ==
                   VerificationResult::Success,
               "正确验证码应验证成功"
           ) &&
           Expect(
               manager.VerifyCode("user@example.com", "register", generation.code) ==
                   VerificationResult::AlreadyUsed,
               "验证成功后验证码应失效"
           );
}

bool TestCodeIsBoundToEmailAndPurpose() {
    // 同一个明文验证码不能跨邮箱或跨业务用途使用。
    VerificationCodeManager manager;
    const auto generation = manager.GenerateCode("user@example.com", "register");
    return Expect(
               generation.status == GenerationStatus::Success,
               "首次生成应成功"
           ) &&
           Expect(
               manager.VerifyCode("other@example.com", "register", generation.code) ==
                   VerificationResult::NotFound,
               "验证码不能用于其他邮箱"
           ) &&
           Expect(
               manager.VerifyCode("user@example.com", "reset_password", generation.code) ==
                   VerificationResult::NotFound,
               "验证码不能用于其他用途"
           );
}

bool TestPerEmailGenerationLimit() {
    // 测试使用一秒冷却期，验证逻辑与默认的一分钟限制相同。
    VerificationCodeManager manager(std::chrono::seconds(1));
    const auto first = manager.GenerateCode("user@example.com", "register");
    const auto same_purpose = manager.GenerateCode("user@example.com", "register");
    const auto other_purpose = manager.GenerateCode("user@example.com", "reset_password");
    const auto other_email = manager.GenerateCode("other@example.com", "register");

    bool success = true;
    success = Expect(first.status == GenerationStatus::Success, "首次生成应成功") && success;
    success = Expect(
        same_purpose.status == GenerationStatus::TooFrequent,
        "同一邮箱同一用途在冷却期内应被限制"
    ) && success;
    success = Expect(
        same_purpose.code.empty(),
        "被限流时不应返回新的验证码"
    ) && success;
    success = Expect(
        same_purpose.retry_after == std::chrono::seconds(1),
        "限流结果应返回剩余等待秒数"
    ) && success;
    success = Expect(
        other_purpose.status == GenerationStatus::TooFrequent,
        "同一邮箱不同用途在冷却期内也应被限制"
    ) && success;
    success = Expect(
        other_email.status == GenerationStatus::Success,
        "不同邮箱不应互相限制"
    ) && success;

    // 等待测试冷却期结束后，同一邮箱应可以再次生成。
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    const auto after_cooldown = manager.GenerateCode(
        "user@example.com",
        "reset_password"
    );
    success = Expect(
        after_cooldown.status == GenerationStatus::Success,
        "冷却期结束后应允许再次生成"
    ) && success;
    return success;
}

bool TestConcurrentGenerationLimit() {
    // 多个线程同时请求同一邮箱时，只允许其中一个线程生成成功。
    VerificationCodeManager manager;
    constexpr int kThreadCount = 8;
    std::atomic<bool> start{false};
    std::atomic<int> success_count{0};
    std::atomic<int> limited_count{0};
    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    // 先创建所有线程，再用 start 让它们尽可能同时进入 GenerateCode。
    for(int index = 0; index < kThreadCount; ++index) {
        workers.emplace_back([&manager, &start, &success_count, &limited_count]() {
            while(!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            const auto result = manager.GenerateCode("user@example.com", "register");
            if(result.status == GenerationStatus::Success) {
                success_count.fetch_add(1, std::memory_order_relaxed);
            }
            else if(result.status == GenerationStatus::TooFrequent) {
                limited_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // 放行线程并等待全部请求结束，再统计成功和限流数量。
    start.store(true, std::memory_order_release);
    for(auto& worker : workers) {
        worker.join();
    }

    return Expect(success_count.load() == 1, "并发请求中只能有一次生成成功") &&
           Expect(
               limited_count.load() == kThreadCount - 1,
               "其余并发请求都应被限流"
           );
}

bool TestAttemptLimit() {
    // 将上限设为两次，快速验证错误次数扣减和封锁行为。
    VerificationCodeManager manager;
    const auto generation = manager.GenerateCode(
        "user@example.com",
        "register",
        std::chrono::minutes(5),
        2
    );

    return Expect(
               generation.status == GenerationStatus::Success,
               "首次生成应成功"
           ) &&
           Expect(
               manager.VerifyCode("user@example.com", "register", "wrong") ==
                   VerificationResult::Incorrect,
               "第一次错误验证应返回 Incorrect"
           ) &&
           Expect(
               manager.VerifyCode("user@example.com", "register", "wrong") ==
                   VerificationResult::TooManyAttempts,
               "达到尝试上限时应返回 TooManyAttempts"
           );
}

bool TestExpiration() {
    // 使用一秒有效期，并在过期后验证 Expired 分支。
    VerificationCodeManager manager;
    const auto generation = manager.GenerateCode(
        "user@example.com",
        "register",
        std::chrono::seconds(1)
    );
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    return Expect(
        generation.status == GenerationStatus::Success,
        "首次生成应成功"
    ) &&
    Expect(
        manager.VerifyCode("user@example.com", "register", generation.code) ==
            VerificationResult::Expired,
        "超过有效期后应返回 Expired"
    );
}

} // namespace

int main() {
    // 分别运行每个用例，避免前一个失败后跳过其余测试。
    bool success = true;
    success = TestGeneratedCodeFormat() && success;
    success = TestSuccessfulVerificationIsOneTime() && success;
    success = TestCodeIsBoundToEmailAndPurpose() && success;
    success = TestPerEmailGenerationLimit() && success;
    success = TestConcurrentGenerationLimit() && success;
    success = TestAttemptLimit() && success;
    success = TestExpiration() && success;

    // 所有规则测试均通过时，测试程序才以状态码 0 退出。
    if(!success) {
        return 1;
    }

    std::cout << "验证码生成与验证测试全部通过\n";
    return 0;
}
