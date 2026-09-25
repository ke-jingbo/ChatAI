#include <curl/curl.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include "VerificationCode.h"

namespace {

// 保存待上传的完整邮件，以及当前已经读取到的位置。
// libcurl 会多次调用读取回调，所以需要用 offset 记录进度。
struct UploadContext {
    const std::string* content;
    std::size_t offset;
};

// libcurl 通过这个回调分批读取邮件内容。
// 返回值表示本次写入 buffer 的字节数；返回 0 表示邮件内容已经读取完毕。
std::size_t ReadEmailData(char* buffer, std::size_t size,
                          std::size_t count, void* user_data) {
    auto* context = static_cast<UploadContext*>(user_data);
    const std::size_t buffer_size = size * count;

    if(buffer_size == 0 || context->offset >= context->content->size()) {
        return 0;
    }

    const std::size_t remaining = context->content->size() - context->offset;
    const std::size_t copy_size = std::min(buffer_size, remaining);

    std::memcpy(buffer, context->content->data() + context->offset, copy_size);
    context->offset += copy_size;
    return copy_size;
}

// 组装一封最基础的纯文本邮件。
// 邮件头和正文之间必须保留一个空行，每一行使用 SMTP 规定的 \r\n 结尾。
std::string BuildVerificationEmail(const std::string& sender,
                                   const std::string& recipient,
                                   const std::string& verify_code) {
    return
        "From: Talking Kibofish <" + sender + ">\r\n"
        "To: <" + recipient + ">\r\n"
        "Subject: Talking Kibofish verification code\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: text/plain; charset=UTF-8\r\n"
        "Content-Transfer-Encoding: 8bit\r\n"
        "\r\n"
        "Your Talking Kibofish verification code is: \r\n[ " + verify_code + " ]\r\n"
        "The code is valid for 5 minutes.\r\n";
}

// 使用 QQ 邮箱的 SMTPS 服务发送验证码邮件。
// sender 是你的发件 QQ 邮箱，auth_code 是该邮箱生成的 SMTP 授权码，
// recipient 是用户填写的收件邮箱。
bool SendQQVerificationEmail(const std::string& sender,
                             const std::string& auth_code,
                             const std::string& recipient,
                             const std::string& verify_code) {
    CURL* curl = curl_easy_init();
    if(curl == nullptr) {
        std::cerr << "创建 CURL 请求失败\n";
        return false;
    }

    // SMTP 的 MAIL FROM 和 RCPT TO 地址使用尖括号包围。
    const std::string mail_from = "<" + sender + ">";
    const std::string mail_to = "<" + recipient + ">";
    const std::string email = BuildVerificationEmail(sender, recipient, verify_code);
    UploadContext upload_context{&email, 0};

    // 一个邮件可以有多个收件人。此示例只添加一个收件人。
    // 此处为收件人列表
    curl_slist* recipients = curl_slist_append(nullptr, mail_to.c_str());
    if(recipients == nullptr) {
        std::cerr << "创建收件人列表失败\n";
        curl_easy_cleanup(curl);
        return false;
    }

    char error_buffer[CURL_ERROR_SIZE] = {0};

    // 1. 连接 QQ 邮箱 SMTP 服务器。
    // smtps 和 465 端口表示从连接开始就使用 TLS 加密。
    curl_easy_setopt(curl, CURLOPT_URL, "smtps://smtp.qq.com:465");

    // 2. 使用完整 QQ 邮箱地址和授权码登录。
    // 这里的 auth_code 不是 QQ 登录密码，也不是发给用户的六位验证码。
    curl_easy_setopt(curl, CURLOPT_USERNAME, sender.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, auth_code.c_str());

    // 3. 设置 SMTP 信封中的发件人和收件人。
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, mail_from.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

    // 4. 告诉 libcurl 从回调函数读取邮件内容并上传到 SMTP 服务器。
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, ReadEmailData);
    curl_easy_setopt(curl, CURLOPT_READDATA, &upload_context);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(email.size()));
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    // 5. 验证 QQ SMTP 服务器的 TLS 证书，不能为了省事关闭证书验证。
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    // 6. 设置超时，防止网络异常时测试程序永久阻塞。
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);

    // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_LOGIN_OPTIONS, "AUTH=LOGIN");

    // 7. 真正建立连接、完成认证并发送邮件。
    const CURLcode result = curl_easy_perform(curl);
    bool success = (result == CURLE_OK);

    if(!success) {
        const char* message = error_buffer[0] != '\0'
            ? error_buffer
            : curl_easy_strerror(result);
        std::cerr << "邮件发送失败：" << message << '\n';
    }

    // 8. 释放本次请求使用的收件人列表和 CURL 对象。
    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);
    return success;
}

} // namespace

int main(int argc, char* argv[]) {
    // 命令行第一个参数是用户邮箱，第二个参数是准备发送的验证码。
    if(argc != 2) {
        std::cerr << "使用方法：" << argv[0] << " <收件邮箱>\n";
        return 1;
    }

    // 发件邮箱和授权码属于服务器秘密，从环境变量读取，不能写死在源码中。
    const char* sender = std::getenv("QQ_SMTP_USER");
    const char* auth_code = std::getenv("QQ_SMTP_AUTH_CODE");
    if(sender == nullptr || auth_code == nullptr) {
        std::cerr
            << "请先设置 QQ_SMTP_USER 和 QQ_SMTP_AUTH_CODE 环境变量\n";
        return 1;
    }

    // libcurl 的全局初始化在进程启动后执行一次。
    const CURLcode init_result = curl_global_init(CURL_GLOBAL_DEFAULT);
    if(init_result != CURLE_OK) {
        std::cerr << "初始化 libcurl 失败："
                  << curl_easy_strerror(init_result) << '\n';
        return 1;
    }

    const bool success = SendQQVerificationEmail(
        sender,
        auth_code,
        argv[1],
        chat_ai::test::VerificationCodeManager().GenerateCode(argv[1], "regist").code
    );

    // 与全局初始化对应；实际服务器中应在进程退出时调用。
    curl_global_cleanup();

    if(!success) {
        return 1;
    }

    std::cout << "邮件发送成功\n";
    return 0;
}
