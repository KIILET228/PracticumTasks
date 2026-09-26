#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <map>

#include "../src/app/use_cases_impl.h"
#include "../src/domain/author.h"
#include "../src/domain/book.h"

namespace {

struct MockAuthorRepository : domain::AuthorRepository {
    std::vector<domain::Author> saved_authors;

    void Save(const domain::Author& author) override {
        saved_authors.emplace_back(author);
    }

    void Delete(const domain::AuthorId& id) override {
        saved_authors.erase(std::remove_if(saved_authors.begin(), saved_authors.end(),
                                           [&](const domain::Author& a) { return a.GetId() == id; }),
                            saved_authors.end());
    }

    std::vector<domain::Author> GetAllAuthors() const override {
        return saved_authors;
    }

    std::optional<domain::Author> GetByName(const std::string& name) const override {
        for (const auto& author : saved_authors) {
            if (author.GetName() == name) {
                return author;
            }
        }
        return std::nullopt;
    }

    std::optional<domain::Author> GetById(const domain::AuthorId& id) const override {
        for (const auto& author : saved_authors) {
            if (author.GetId() == id) {
                return author;
            }
        }
        return std::nullopt;
    }
};

struct MockBookRepository : domain::BookRepository {
    std::vector<domain::Book> saved_books;
    std::map<std::string, std::vector<std::string>> tags_by_book;

    void Save(const domain::Book& book) override {
        saved_books.emplace_back(book);
    }

    void Delete(const domain::BookId& id) override {
        saved_books.erase(std::remove_if(saved_books.begin(), saved_books.end(),
                                         [&](const domain::Book& b) { return b.GetId() == id; }),
                          saved_books.end());
        tags_by_book.erase(id.ToString());
    }

    void UpdateTitleAndYear(const domain::BookId& id, const std::string& title, int year) override {
        for (auto& book : saved_books) {
            if (book.GetId() == id) {
                book = domain::Book{book.GetId(), book.GetAuthorId(), title, year};
                break;
            }
        }
    }

    void SetTags(const domain::BookId& id, const std::vector<std::string>& tags) override {
        tags_by_book[id.ToString()] = tags;
    }

    std::vector<std::string> GetTags(const domain::BookId& id) const override {
        auto it = tags_by_book.find(id.ToString());
        return it != tags_by_book.end() ? it->second : std::vector<std::string>{};
    }

    std::vector<domain::Book> GetAllBooks() const override {
        return saved_books;
    }

    std::vector<domain::Book> GetAuthorBooks(const domain::AuthorId& author_id) const override {
        std::vector<domain::Book> result;
        for (const auto& book : saved_books) {
            if (book.GetAuthorId() == author_id) {
                result.push_back(book);
            }
        }
        return result;
    }

    std::vector<domain::Book> FindByTitle(const std::string& title) const override {
        std::vector<domain::Book> result;
        for (const auto& book : saved_books) {
            if (book.GetTitle() == title) {
                result.push_back(book);
            }
        }
        return result;
    }

    std::optional<domain::Book> GetById(const domain::BookId& id) const override {
        for (const auto& book : saved_books) {
            if (book.GetId() == id) {
                return book;
            }
        }
        return std::nullopt;
    }
};

struct Fixture {
    MockAuthorRepository authors;
    MockBookRepository books;
};

}  // namespace

SCENARIO_METHOD(Fixture, "Book Adding") {
    GIVEN("Use cases") {
        app::UseCasesImpl use_cases{authors, books};

        WHEN("Adding an author") {
            const auto author_name = "Joanne Rowling";
            use_cases.AddAuthor(author_name);

            THEN("author with the specified name is saved to repository") {
                REQUIRE(authors.saved_authors.size() == 1);
                CHECK(authors.saved_authors.at(0).GetName() == author_name);
                CHECK(authors.saved_authors.at(0).GetId() != domain::AuthorId{});
            }
        }

        WHEN("Adding a book") {
            use_cases.AddAuthor("Herman Melville");
            const auto author_id = authors.saved_authors.at(0).GetId().ToString();

            use_cases.AddBook(author_id, "Moby-Dick", 1851, "");

            THEN("book with the specified data is saved to repository") {
                REQUIRE(books.saved_books.size() == 1);
                CHECK(books.saved_books.at(0).GetTitle() == "Moby-Dick");
                CHECK(books.saved_books.at(0).GetPublicationYear() == 1851);
                CHECK(books.saved_books.at(0).GetAuthorId().ToString() == author_id);
            }
        }
    }
}
