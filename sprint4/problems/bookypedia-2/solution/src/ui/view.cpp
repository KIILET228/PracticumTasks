#include "view.h"

#include <boost/algorithm/string/trim.hpp>
#include <cstddef>
#include <functional>
#include <iostream>
#include <optional>
#include <string>

#include "../menu/menu.h"

using namespace std::literals;
namespace ph = std::placeholders;

namespace ui {

namespace {

// Печатает пронумерованный список строк в формате "N значение" (без точки).
template <typename T>
void PrintVector(std::ostream& out, const std::vector<T>& vector) {
    int i = 1;
    for (auto& value : vector) {
        out << i++ << " " << value << std::endl;
    }
}

}  // namespace

View::View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output)
    : menu_{menu}
    , use_cases_{use_cases}
    , input_{input}
    , output_{output} {
    menu_.AddAction("AddAuthor"s, "name"s, "Adds author"s, std::bind(&View::AddAuthor, this, ph::_1));
    menu_.AddAction("DeleteAuthor"s, "[name]"s, "Deletes author"s,
                    std::bind(&View::DeleteAuthor, this, ph::_1));
    menu_.AddAction("EditAuthor"s, "[name]"s, "Edits author name"s,
                    std::bind(&View::EditAuthor, this, ph::_1));
    menu_.AddAction("ShowAuthors"s, {}, "Show authors"s, std::bind(&View::ShowAuthors, this));
    menu_.AddAction("ShowAuthorBooks"s, {}, "Show author books"s,
                    std::bind(&View::ShowAuthorBooks, this));

    menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book"s,
                    std::bind(&View::AddBook, this, ph::_1));
    menu_.AddAction("ShowBooks"s, {}, "Show books"s, std::bind(&View::ShowBooks, this));
    menu_.AddAction("ShowBook"s, "[title]"s, "Show book info"s, std::bind(&View::ShowBook, this, ph::_1));
    menu_.AddAction("DeleteBook"s, "[title]"s, "Deletes book"s, std::bind(&View::DeleteBook, this, ph::_1));
    menu_.AddAction("EditBook"s, "[title]"s, "Edits book"s, std::bind(&View::EditBook, this, ph::_1));
}

// ---------------------------- Авторы ----------------------------

bool View::AddAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        use_cases_.AddAuthor(name);
    } catch (const std::exception&) {
        output_ << "Failed to add author"sv << std::endl;
    }
    return true;
}

bool View::DeleteAuthor(std::istream& cmd_input) const {
    std::string name;
    std::getline(cmd_input, name);
    boost::algorithm::trim(name);

    if (!name.empty()) {
        try {
            use_cases_.DeleteAuthorByName(name);
        } catch (const std::exception&) {
            output_ << "Failed to delete author"sv << std::endl;
        }
        return true;
    }

    try {
        if (auto author_id = SelectAuthor()) {
            use_cases_.DeleteAuthorById(*author_id);
        }
    } catch (const std::exception&) {
        // Некорректный ввод при выборе автора - молча игнорируем.
    }
    return true;
}

bool View::EditAuthor(std::istream& cmd_input) const {
    std::string name;
    std::getline(cmd_input, name);
    boost::algorithm::trim(name);

    std::optional<std::string> author_id;

    if (!name.empty()) {
        auto found = use_cases_.FindAuthorByName(name);
        if (!found) {
            output_ << "Failed to edit author"sv << std::endl;
            return true;
        }
        author_id = found->id;
    } else {
        try {
            author_id = SelectAuthor();
        } catch (const std::exception&) {
            author_id = std::nullopt;
        }
        if (!author_id) {
            return true;
        }
    }

    output_ << "Enter new name:"sv << std::endl;
    std::string new_name;
    if (!std::getline(input_, new_name)) {
        return true;
    }
    boost::algorithm::trim(new_name);

    try {
        use_cases_.EditAuthorName(*author_id, new_name);
    } catch (const std::exception&) {
        output_ << "Failed to edit author"sv << std::endl;
    }
    return true;
}

bool View::ShowAuthors() const {
    std::vector<std::string> names;
    for (auto& author : use_cases_.GetAuthors()) {
        names.push_back(author.name);
    }
    PrintVector(output_, names);
    return true;
}

std::optional<std::string> View::SelectAuthor() const {
    output_ << "Select author:"sv << std::endl;
    auto authors = use_cases_.GetAuthors();
    std::vector<std::string> names;
    for (auto& author : authors) {
        names.push_back(author.name);
    }
    PrintVector(output_, names);
    output_ << "Enter author # or empty line to cancel"sv << std::endl;

    std::string str;
    if (!std::getline(input_, str) || str.empty()) {
        return std::nullopt;
    }

    int author_idx;
    try {
        author_idx = std::stoi(str);
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid author num");
    }

    --author_idx;
    if (author_idx < 0 || static_cast<std::size_t>(author_idx) >= authors.size()) {
        throw std::runtime_error("Invalid author num");
    }

    return authors[author_idx].id;
}

bool View::ShowAuthorBooks() const {
    try {
        if (auto author_id = SelectAuthor()) {
            std::vector<std::string> lines;
            for (auto& book : use_cases_.GetAuthorBooks(*author_id)) {
                lines.push_back(book.title + ", " + std::to_string(book.publication_year));
            }
            PrintVector(output_, lines);
        }
    } catch (const std::exception&) {
        // Некорректный номер автора - ничего не выводим.
    }
    return true;
}

// ---------------------------- Книги ----------------------------

void View::PrintBookLine(int index, const app::BookInfo& book) const {
    output_ << index << " " << book.title << " by " << book.author_name << ", " << book.publication_year
            << std::endl;
}

bool View::AddBook(std::istream& cmd_input) const {
    try {
        int year = 0;
        std::string title;
        cmd_input >> year;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        output_ << "Enter author name or empty line to select from list:"sv << std::endl;
        std::string author_name;
        std::getline(input_, author_name);
        boost::algorithm::trim(author_name);

        std::string author_id;

        if (!author_name.empty()) {
            auto found = use_cases_.FindAuthorByName(author_name);
            if (found) {
                author_id = found->id;
            } else {
                output_ << "No author found. Do you want to add "sv << author_name << " (y/n)?"sv
                        << std::endl;
                std::string answer;
                std::getline(input_, answer);
                boost::algorithm::trim(answer);
                if (answer == "y" || answer == "Y") {
                    author_id = use_cases_.AddAuthor(author_name);
                } else {
                    output_ << "Failed to add book"sv << std::endl;
                    return true;
                }
            }
        } else {
            auto selected = SelectAuthor();
            if (!selected) {
                return true;  // Отмена - ничего не выводим.
            }
            author_id = *selected;
        }

        output_ << "Enter tags (comma separated):"sv << std::endl;
        std::string tags;
        std::getline(input_, tags);

        use_cases_.AddBook(author_id, title, year, tags);
    } catch (const std::exception&) {
        output_ << "Failed to add book"sv << std::endl;
    }
    return true;
}

bool View::ShowBooks() const {
    auto books = use_cases_.GetBooks();
    int i = 1;
    for (auto& book : books) {
        PrintBookLine(i++, book);
    }
    return true;
}

View::BookSelectResult View::ChooseFromBookList(const std::vector<app::BookInfo>& books) const {
    if (books.empty()) {
        return {BookSelectOutcome::kNotFound, {}};
    }

    int i = 1;
    for (auto& book : books) {
        PrintBookLine(i++, book);
    }
    output_ << "Enter the book # or empty line to cancel:"sv << std::endl;

    std::string str;
    if (!std::getline(input_, str)) {
        return {BookSelectOutcome::kCanceled, {}};
    }
    boost::algorithm::trim(str);
    if (str.empty()) {
        return {BookSelectOutcome::kCanceled, {}};
    }

    int idx;
    try {
        idx = std::stoi(str);
    } catch (const std::exception&) {
        return {BookSelectOutcome::kNotFound, {}};
    }

    --idx;
    if (idx < 0 || static_cast<std::size_t>(idx) >= books.size()) {
        return {BookSelectOutcome::kNotFound, {}};
    }

    return {BookSelectOutcome::kSelected, books[idx].id};
}

View::BookSelectResult View::SelectBook(const std::string& title) const {
    if (!title.empty()) {
        auto books = use_cases_.FindBooksByTitle(title);
        if (books.empty()) {
            return {BookSelectOutcome::kNotFound, {}};
        }
        if (books.size() == 1) {
            // Единственное совпадение - действуем сразу, без списка выбора.
            return {BookSelectOutcome::kSelected, books.front().id};
        }
        return ChooseFromBookList(books);
    }
    // Название не указано - всегда показываем полный список книг.
    return ChooseFromBookList(use_cases_.GetBooks());
}

bool View::ShowBook(std::istream& cmd_input) const {
    std::string title;
    std::getline(cmd_input, title);
    boost::algorithm::trim(title);

    auto result = SelectBook(title);
    if (result.outcome != BookSelectOutcome::kSelected) {
        // Книга не найдена или выбор отменён - ничего не выводим.
        return true;
    }

    auto details = use_cases_.GetBookDetails(result.book_id);
    if (!details) {
        return true;
    }

    output_ << "Title: "sv << details->title << std::endl;
    output_ << "Author: "sv << details->author_name << std::endl;
    output_ << "Publication year: "sv << details->publication_year << std::endl;
    if (!details->tags.empty()) {
        output_ << "Tags: "sv;
        bool first = true;
        for (auto& tag : details->tags) {
            if (!first) {
                output_ << ", "sv;
            }
            output_ << tag;
            first = false;
        }
        output_ << std::endl;
    }
    return true;
}

bool View::DeleteBook(std::istream& cmd_input) const {
    std::string title;
    std::getline(cmd_input, title);
    boost::algorithm::trim(title);

    auto result = SelectBook(title);
    if (result.outcome == BookSelectOutcome::kCanceled) {
        return true;  // Отмена - ничего не выводим.
    }
    if (result.outcome == BookSelectOutcome::kNotFound) {
        output_ << "Book not found"sv << std::endl;
        return true;
    }

    try {
        use_cases_.DeleteBook(result.book_id);
    } catch (const std::exception&) {
        output_ << "Failed to delete book"sv << std::endl;
    }
    return true;
}

bool View::EditBook(std::istream& cmd_input) const {
    std::string title;
    std::getline(cmd_input, title);
    boost::algorithm::trim(title);

    auto result = SelectBook(title);
    if (result.outcome != BookSelectOutcome::kSelected) {
        output_ << "Book not found"sv << std::endl;
        return true;
    }

    auto current = use_cases_.GetBookDetails(result.book_id);
    if (!current) {
        output_ << "Book not found"sv << std::endl;
        return true;
    }

    output_ << "Enter new title or empty line to use the current one ("sv << current->title << "):"sv
            << std::endl;
    std::string new_title;
    std::getline(input_, new_title);
    boost::algorithm::trim(new_title);

    output_ << "Enter publication year or empty line to use the current one ("sv
            << current->publication_year << "):"sv << std::endl;
    std::string new_year_str;
    std::getline(input_, new_year_str);
    boost::algorithm::trim(new_year_str);

    std::string current_tags_joined;
    for (std::size_t i = 0; i < current->tags.size(); ++i) {
        if (i != 0) {
            current_tags_joined += ", "sv;
        }
        current_tags_joined += current->tags[i];
    }
    output_ << "Enter tags (current tags: "sv << current_tags_joined << "):"sv << std::endl;
    std::string new_tags;
    std::getline(input_, new_tags);

    try {
        std::optional<std::string> title_opt;
        if (!new_title.empty()) {
            title_opt = new_title;
        }

        std::optional<int> year_opt;
        if (!new_year_str.empty()) {
            year_opt = std::stoi(new_year_str);
        }

        use_cases_.EditBook(result.book_id, title_opt, year_opt, new_tags);
    } catch (const std::exception&) {
        output_ << "Book not found"sv << std::endl;
    }
    return true;
}

}  // namespace ui
