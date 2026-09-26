#include "use_cases_impl.h"

#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include <cctype>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {
using namespace domain;

namespace {

// Обрезает пробелы по краям строки и схлопывает внутренние повторяющиеся
// пробельные символы в один пробел (например, "  gold   rush  " -> "gold rush").
std::string NormalizeWhitespace(std::string s) {
    boost::algorithm::trim(s);
    std::string result;
    result.reserve(s.size());
    bool last_was_space = false;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!last_was_space) {
                result += ' ';
            }
            last_was_space = true;
        } else {
            result += c;
            last_was_space = false;
        }
    }
    return result;
}

// Разбивает строку тегов через запятую, нормализует пробелы в каждом теге,
// отбрасывает пустые теги и удаляет дубликаты. Возвращает теги, отсортированные
// по возрастанию (как std::set<std::string> с оператором < по умолчанию).
std::vector<std::string> ParseTags(const std::string& raw_tags) {
    std::set<std::string> unique_tags;
    std::istringstream stream(raw_tags);
    std::string tag;
    while (std::getline(stream, tag, ',')) {
        std::string normalized = NormalizeWhitespace(tag);
        if (!normalized.empty()) {
            unique_tags.insert(normalized);
        }
    }
    return {unique_tags.begin(), unique_tags.end()};
}

std::unordered_map<std::string, std::string> BuildAuthorNameMap(const AuthorRepository& authors) {
    std::unordered_map<std::string, std::string> result;
    for (const auto& author : authors.GetAllAuthors()) {
        result[author.GetId().ToString()] = author.GetName();
    }
    return result;
}

BookInfo ToBookInfo(const Book& book, const std::unordered_map<std::string, std::string>& author_names) {
    BookInfo info;
    info.id = book.GetId().ToString();
    info.title = book.GetTitle();
    info.author_id = book.GetAuthorId().ToString();
    auto it = author_names.find(info.author_id);
    info.author_name = it != author_names.end() ? it->second : std::string{};
    info.publication_year = book.GetPublicationYear();
    return info;
}

}  // namespace

std::string UseCasesImpl::AddAuthor(const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("Author name is empty");
    }
    auto id = AuthorId::New();
    authors_.Save({id, name});
    return id.ToString();
}

std::vector<AuthorInfo> UseCasesImpl::GetAuthors() {
    std::vector<AuthorInfo> result;
    for (const auto& author : authors_.GetAllAuthors()) {
        result.push_back({author.GetId().ToString(), author.GetName()});
    }
    return result;
}

std::optional<AuthorInfo> UseCasesImpl::FindAuthorByName(const std::string& name) {
    auto author = authors_.GetByName(name);
    if (!author) {
        return std::nullopt;
    }
    return AuthorInfo{author->GetId().ToString(), author->GetName()};
}

void UseCasesImpl::DeleteAuthorByName(const std::string& name) {
    auto author = authors_.GetByName(name);
    if (!author) {
        throw std::runtime_error("Author not found");
    }
    authors_.Delete(author->GetId());
}

void UseCasesImpl::DeleteAuthorById(const std::string& author_id) {
    authors_.Delete(AuthorId::FromString(author_id));
}

void UseCasesImpl::EditAuthorName(const std::string& author_id, const std::string& new_name) {
    if (new_name.empty()) {
        throw std::invalid_argument("New author name is empty");
    }
    authors_.Save({AuthorId::FromString(author_id), new_name});
}

void UseCasesImpl::AddBook(const std::string& author_id, const std::string& title, int publication_year,
                           const std::string& raw_tags) {
    if (title.empty()) {
        throw std::invalid_argument("Book title is empty");
    }
    auto id = BookId::New();
    books_.Save({id, AuthorId::FromString(author_id), title, publication_year});
    books_.SetTags(id, ParseTags(raw_tags));
}

std::vector<BookInfo> UseCasesImpl::GetBooks() {
    auto author_names = BuildAuthorNameMap(authors_);

    std::vector<BookInfo> result;
    for (const auto& book : books_.GetAllBooks()) {
        result.push_back(ToBookInfo(book, author_names));
    }
    std::sort(result.begin(), result.end(), [](const BookInfo& a, const BookInfo& b) {
        if (a.title != b.title) {
            return a.title < b.title;
        }
        if (a.author_name != b.author_name) {
            return a.author_name < b.author_name;
        }
        return a.publication_year < b.publication_year;
    });
    return result;
}

std::vector<BookInfo> UseCasesImpl::GetAuthorBooks(const std::string& author_id) {
    auto author_names = BuildAuthorNameMap(authors_);
    std::vector<BookInfo> result;
    for (const auto& book : books_.GetAuthorBooks(AuthorId::FromString(author_id))) {
        result.push_back(ToBookInfo(book, author_names));
    }
    return result;
}

std::vector<BookInfo> UseCasesImpl::FindBooksByTitle(const std::string& title) {
    auto author_names = BuildAuthorNameMap(authors_);
    std::vector<BookInfo> result;
    for (const auto& book : books_.FindByTitle(title)) {
        result.push_back(ToBookInfo(book, author_names));
    }
    std::sort(result.begin(), result.end(),
             [](const BookInfo& a, const BookInfo& b) { return a.author_name < b.author_name; });
    return result;
}

std::optional<BookDetails> UseCasesImpl::GetBookDetails(const std::string& book_id) {
    auto id = BookId::FromString(book_id);
    auto book = books_.GetById(id);
    if (!book) {
        return std::nullopt;
    }
    auto author = authors_.GetById(book->GetAuthorId());

    BookDetails details;
    details.title = book->GetTitle();
    details.author_name = author ? author->GetName() : std::string{};
    details.publication_year = book->GetPublicationYear();
    details.tags = books_.GetTags(id);
    return details;
}

void UseCasesImpl::DeleteBook(const std::string& book_id) {
    books_.Delete(BookId::FromString(book_id));
}

void UseCasesImpl::EditBook(const std::string& book_id, std::optional<std::string> new_title,
                            std::optional<int> new_year, const std::string& raw_tags) {
    auto id = BookId::FromString(book_id);
    auto book = books_.GetById(id);
    if (!book) {
        throw std::runtime_error("Book not found");
    }

    const std::string title = new_title.value_or(book->GetTitle());
    const int year = new_year.value_or(book->GetPublicationYear());
    books_.UpdateTitleAndYear(id, title, year);

    // Теги всегда заменяются переданным набором (в том числе пустым).
    books_.SetTags(id, ParseTags(raw_tags));
}

}  // namespace app
