#pragma once
#include <optional>
#include <string>
#include <vector>

#include "author.h"
#include "../util/tagged_uuid.h"

namespace domain {

namespace detail {
struct BookTag {};
}  // namespace detail

using BookId = util::TaggedUUID<detail::BookTag>;

class Book {
public:
    Book(BookId id, AuthorId author_id, std::string title, int publication_year)
        : id_(std::move(id))
        , author_id_(std::move(author_id))
        , title_(std::move(title))
        , publication_year_(publication_year) {
    }

    const BookId& GetId() const noexcept {
        return id_;
    }

    const AuthorId& GetAuthorId() const noexcept {
        return author_id_;
    }

    const std::string& GetTitle() const noexcept {
        return title_;
    }

    int GetPublicationYear() const noexcept {
        return publication_year_;
    }

private:
    BookId id_;
    AuthorId author_id_;
    std::string title_;
    int publication_year_;
};

class BookRepository {
public:
    virtual void Save(const Book& book) = 0;
    virtual void Delete(const BookId& id) = 0;
    virtual void UpdateTitleAndYear(const BookId& id, const std::string& title, int year) = 0;

    // Полностью заменяет набор тегов книги (удаляет старые, вставляет новые).
    virtual void SetTags(const BookId& id, const std::vector<std::string>& tags) = 0;
    // Возвращает теги книги, отсортированные по возрастанию.
    virtual std::vector<std::string> GetTags(const BookId& id) const = 0;

    virtual std::vector<Book> GetAllBooks() const = 0;

    // Возвращает книги указанного автора, отсортированные по году издания,
    // а при совпадении года - по названию (обе сортировки по возрастанию).
    virtual std::vector<Book> GetAuthorBooks(const AuthorId& author_id) const = 0;

    // Возвращает все книги с точным совпадением названия (регистрозависимо).
    virtual std::vector<Book> FindByTitle(const std::string& title) const = 0;

    virtual std::optional<Book> GetById(const BookId& id) const = 0;

protected:
    ~BookRepository() = default;
};

}  // namespace domain
