#include "bot/app.h"
#include "bot/chat.h"

ChatBotApp::ChatBotApp(const std::string& file_name)
:config{read_json(file_name)},
db{config["db_file"].GetString()},
bot{config["token"].GetString()},
tm{config["text_storage_file"].GetString()}
{}

void ChatBotApp::start()
{
	std::cout << "Bot started\n";

	bot.infinit_polling();
	
	// try {
	// 	bot.infinit_polling();
	// } catch (const std::exception& e) {
	// 	std::cerr << "Бот приуныл: " << e.what() << "\n";
	// }

	db.write(config["db_file"].GetString());

	std::cout << "Bot finished\n";
}