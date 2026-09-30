#pragma once
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "../app/use_cases.h"

namespace menu {
class Menu;
}

namespace app {
class UseCases;
}

namespace ui {

class View {
public:
    View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output);

private:
    enum class BookSelectOutcome {
        kSelected,
        kCanceled,
        kNotFound,
    };

    struct BookSelectResult {
        BookSelectOutcome outcome = BookSelectOutcome::kNotFound;
        std::string book_id;
    };

    // Авторы
    bool AddAuthor(std::istream& cmd_input) const;
    bool DeleteAuthor(std::istream& cmd_input) const;
    bool EditAuthor(std::istream& cmd_input) const;
    bool ShowAuthors() const;
    bool ShowAuthorBooks() const;

    // Книги
    bool AddBook(std::istream& cmd_input) const;
    bool ShowBooks() const;
    bool ShowBook(std::istream& cmd_input) const;
    bool DeleteBook(std::istream& cmd_input) const;
    bool EditBook(std::istream& cmd_input) const;

    // Показывает "Select author:" + пронумерованный список + строку-приглашение,
    // считывает номер (или пустую строку для отмены) и возвращает id автора.
    std::optional<std::string> SelectAuthor() const;

    // Показывает пронумерованный список книг (без заголовка) + строку-приглашение,
    // считывает номер (или пустую строку для отмены) и возвращает результат выбора.
    BookSelectResult ChooseFromBookList(const std::vector<app::BookInfo>& books) const;

    // Если title непустой и найдено ровно одно совпадение - книга выбирается сразу,
    // без отображения списка. Если совпадений несколько - показывается список для
    // выбора. Если title пуст - всегда показывается полный список книг (как ShowBooks).
    BookSelectResult SelectBook(const std::string& title) const;

    void PrintBookLine(int index, const app::BookInfo& book) const;

    menu::Menu& menu_;
    app::UseCases& use_cases_;
    std::istream& input_;
    std::ostream& output_;
};

}  // namespace ui
