#include "bot/app.h"
#include <stdexcept>

int main(int argc, char** argv) {
	if (argc < 2)
		throw std::runtime_error("Too few arguments, config file required");

	ChatBotApp app(argv[1]);
	app.start();
    return 0;
}