#include "book_manager.h"

#include <boost/json.hpp>
#include <iostream>
#include <string>

using namespace std::literals;

namespace {

void PrintResult(bool success) {
    boost::json::object response;
    response["result"] = success;
    std::cout << boost::json::serialize(response) << std::endl;
}

}  // namespace

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: book_manager <db-connection-string>\n"sv;
        return EXIT_FAILURE;
    }

    try {
        db::BookManager book_manager{argv[1]};

        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) {
                continue;
            }

            boost::json::value request;
            try {
                request = boost::json::parse(line);
            } catch (const std::exception& ex) {
                std::cerr << "Failed to parse request: "sv << ex.what() << std::endl;
                continue;
            }

            const auto& request_obj = request.as_object();
            const std::string action{request_obj.at("action").as_string()};
            const auto& payload = request_obj.at("payload").as_object();

            if (action == "add_book"sv) {
                PrintResult(book_manager.AddBook(payload));
            } else if (action == "all_books"sv) {
                std::cout << boost::json::serialize(book_manager.GetAllBooks()) << std::endl;
            } else if (action == "exit"sv) {
                break;
            } else {
                std::cerr << "Unknown action: "sv << action << std::endl;
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
