#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <dpp/dpp.h>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <signal.h>

constexpr const char* s_info = R"(Hello! I'm Reminder bot :)
\* created by : <https://github.com/lionkor>
\* source code: <https://github.com/lionkor/reminder-bot>
)";

struct Reminder {
    dpp::snowflake channel_id;
    dpp::snowflake message_id;
    dpp::snowflake guild_id;
    size_t seconds;
    size_t original_seconds;
    std::string message;
    bool repeat;
    dpp::user user;
    // std::vector<dpp::snowflake> mentions;
    int32_t color { std::rand() };
};

std::atomic_bool s_shutdown { false };

std::vector<Reminder> s_reminders {};

extern "C" void signal_handler(int sig) {
    switch (sig) {
    case SIGINT:
        s_shutdown = true;
        break;
    }
}

void setup_commands(dpp::cluster& bot, dpp::snowflake guild_id) {
    fmt::print("setting up commands for '{}'\n", guild_id);
    dpp::slashcommand cmd_remind;
    cmd_remind.set_name("remind")
        .set_description("Reminds you!")
        .set_application_id(bot.me.id)
        .add_option(dpp::command_option(dpp::command_option_type::co_number, "seconds", "time in seconds until reminder", true))
        .add_option(dpp::command_option(dpp::command_option_type::co_string, "message", "what to remind you about", true))
        .add_option(dpp::command_option(dpp::command_option_type::co_boolean, "repeat", "whether to repeat this reminder indefinitely", false));
    bot.guild_bulk_command_create({ cmd_remind }, guild_id, [](const dpp::confirmation_callback_t& cb) {
        if (cb.is_error()) {
            fmt::print("error: {}\n", cb.get_error().message);
        }
    });
}

int main() {
    std::srand(std::time(nullptr));
    signal(SIGINT, signal_handler);
    auto token = std::getenv("DISCORD_BOT_TOKEN");
    if (!token || std::strlen(token) == 0) {
        fmt::print(stderr, "error: DISCORD_BOT_TOKEN environment variable not provided or empty.\n");
        std::exit(1);
    }
    fmt::print("using token of length {}\n", std::strlen(token));
    dpp::cluster bot(token);
    bot.on_interaction_create([&bot](const dpp::interaction_create_t& event) {
        if (event.command.type == dpp::it_application_command) {
            dpp::command_interaction command = std::get<dpp::command_interaction>(event.command.data);

            if (command.name == "info") {
                event.reply(dpp::ir_channel_message_with_source, s_info);
            } else if (command.name == "ping") {
                event.reply(dpp::ir_channel_message_with_source, "Pong!");
            } else if (command.name == "mention") {
                event.reply(dpp::ir_channel_message_with_source, fmt::format("not implemented"));
            } else if (command.name == "remind") {
                size_t seconds {};
                bool repeat { false };
                std::string message {};
                for (const auto& op : command.options) {
                    if (op.type == dpp::command_option_type::co_number) {
                        if (op.name == "seconds") {
                            seconds = std::get<double>(op.value);
                        }
                    } else if (op.type == dpp::command_option_type::co_boolean) {
                        if (op.name == "repeat") {
                            repeat = std::get<bool>(op.value);
                        }
                    } else if (op.type == dpp::command_option_type::co_string) {
                        if (op.name == "message") {
                            message = std::get<std::string>(op.value);
                        }
                    }
                }
                if (seconds < 20) {
                    event.reply(dpp::ir_channel_message_with_source, fmt::format("Please enter >=20 seconds, {} is too spammy!", seconds));
                    fmt::print("invalid time, skipping");
                    return;
                }
                Reminder reminder;
                reminder.seconds = seconds;
                reminder.original_seconds = seconds;
                reminder.repeat = repeat;
                reminder.message = message;
                reminder.message_id = event.command.message_id;
                reminder.channel_id = event.command.channel_id;
                reminder.guild_id = event.command.guild_id;
                reminder.user = event.command.usr;
                /*for (const auto& pair : event.command.resolved.users) {
                    reminder.mentions.push_back(pair.first);
                }*/
                //fmt::print("found {} mentions\n", reminder.mentions.size());
                s_reminders.push_back(std::move(reminder));
                if (repeat) {
                    event.reply(dpp::ir_channel_message_with_source, fmt::format("Reminding you every {} second(s) of '{}'", seconds, message));
                } else {
                    event.reply(dpp::ir_channel_message_with_source, fmt::format("Reminding you in {} second(s) of '{}'", seconds, message));
                }
            } else {
                fmt::print("unhandled command: '{}'\n", command.name);
            }
        } else {
            fmt::print("unhandled interaction: {}\n", event.command.type);
        }
    });
    bot.on_log([&bot](const dpp::log_t& event) {
        switch (event.severity) {
        case dpp::ll_trace:
            fmt::print("dpp: trace: {}\n", event.message);
            break;
        case dpp::ll_debug:
            fmt::print("dpp: debug: {}\n", event.message);
            break;
        case dpp::ll_info:
            fmt::print("dpp: info: {}\n", event.message);
            break;
        case dpp::ll_warning:
            fmt::print("dpp: warning: {}\n", event.message);
            break;
        case dpp::ll_error:
            fmt::print("dpp: error: {}\n", event.message);
            break;
        case dpp::ll_critical:
        default:
            fmt::print("dpp: critical: {}\n", event.message);
            break;
        }
    });
    bot.on_ready([&bot](const dpp::ready_t& event) {
        fmt::print("bot session '{}' is ready as '{}' on shard '{}'\n", event.session_id, bot.me.username, event.shard_id);
        fmt::print("bot invite: 'https://discord.com/api/oauth2/authorize?client_id={}&permissions=199744&scope=bot%20applications.commands'\n", bot.me.id);
        dpp::presence presence(dpp::presence_status::ps_online, dpp::activity_type::at_custom, "the clock");
        bot.set_presence(presence);
    });
    bot.on_guild_create([&bot](const dpp::guild_create_t& guild) {
        fmt::print("joined server {} '{}'\n", guild.created->id, guild.created->name);
        setup_commands(bot, guild.created->id);
    });
    bot.start(true);
    std::vector<decltype(s_reminders)::iterator> to_delete {};

    while (!s_shutdown) {
        auto before = std::chrono::high_resolution_clock::now();
        for (auto iter = s_reminders.begin(); iter != s_reminders.end(); ++iter) {
            auto& reminder = *iter;
            if (reminder.seconds == 0) {
                fmt::print("expired! trying to send in {}\n", reminder.channel_id);
                dpp::embed embed;
                embed.set_title("Reminder")
                    .set_description(reminder.message)
                    .set_footer(dpp::embed_footer().set_text(fmt::format("after {}s", reminder.original_seconds)))
                    .set_color(reminder.color);
                dpp::message msg(reminder.channel_id, embed);
                std::string content;
                content = "<@" + std::to_string(reminder.user.id) + ">";

                /*for (auto& id : reminder.mentions) {
                    content += "<@" + std::to_string(id) + ">";
                }*/
                msg.set_content(content);
                msg.set_allowed_mentions(false, true, true, true, { reminder.user.id }, {});
                fmt::print("trying to send '{}'\n", msg.build_json());
                bot.message_create(msg, [](const dpp::confirmation_callback_t& cb) {
                    if (cb.is_error()) {
                        fmt::print("error: {}\n", cb.get_error().message);
                    }
                });
                if (reminder.repeat) {
                    reminder.seconds = reminder.original_seconds;
                } else {
                    to_delete.push_back(iter);
                    continue;
                }
            }
            --reminder.seconds;
        }
        for (auto iter : to_delete) {
            s_reminders.erase(iter);
        }
        to_delete.clear();
        auto after = std::chrono::high_resolution_clock::now();
        if (after - before < std::chrono::seconds(1)) {
            std::this_thread::sleep_for(std::chrono::seconds(1) - (after - before));
        } else {
            fmt::print("can't keep up!");
        }
    }
    fmt::print("terminating gracefully\n");
    return 0;
}
