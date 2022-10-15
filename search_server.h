#pragma once

#include "document.h"
#include "string_processing.h"
#include "log_duration.h"
#include "concurrent_map.h"

#include <unordered_set>
#include <vector>
#include <algorithm>
#include <set>
#include <map>
#include <stdexcept>
#include <string>
#include <iostream>
#include <cmath>
#include <utility>
#include <execution>
#include <tuple>

#include "document.h" 

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

static auto key_mapper = [](const Document& document) {
    return document.id;
};

class SearchServer {
public:

    template <typename StringCollection>
    explicit SearchServer(const StringCollection& stop_words) {
        for (const auto& word : stop_words) {
            if (!IsValidWord(word)) {
                throw std::invalid_argument("Stop-words contain special symbols");
            }
            stop_words_.insert(std::string(word.data(), word.size()));
        }
    }

    explicit SearchServer(std::string stop_words)
        :SearchServer(SplitIntoWords(stop_words))
    {
    }

    explicit SearchServer(std::string_view stop_words)
        :SearchServer(SplitIntoWordsView(stop_words))
    {
    }

    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename KeyMapper>
    std::vector<Document> FindTopDocuments(const std::string_view query, KeyMapper key_mapper) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus doc_status) const {
        return SearchServer::FindTopDocuments(raw_query, [doc_status](int document_id, DocumentStatus status, int rating) { return status == doc_status; });
    }

    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const {
        return SearchServer::FindTopDocuments(raw_query, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; });
    }

    template <typename KeyMapper>
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, const std::string_view query, KeyMapper key_mapper) const {
        return FindTopDocuments(query, key_mapper);
    }

    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, const std::string_view raw_query, DocumentStatus doc_status) const {
        return SearchServer::FindTopDocuments(raw_query, [doc_status](int document_id, DocumentStatus status, int rating) { return status == doc_status; });
    }

    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, const std::string_view raw_query) const {
        return SearchServer::FindTopDocuments(raw_query, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; });
    }

    template <typename KeyMapper>
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, const std::string_view query, KeyMapper key_mapper) const;

    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, const std::string_view raw_query, DocumentStatus doc_status) const {
        return SearchServer::FindTopDocuments(std::execution::par, raw_query, [doc_status](int document_id, DocumentStatus status, int rating) { return status == doc_status; });
    }

    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, const std::string_view raw_query) const {
        return SearchServer::FindTopDocuments(std::execution::par, raw_query, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; });
    }

    int GetDocumentCount() const;

    set<int>::const_iterator begin() const;

    set<int>::const_iterator end() const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy, std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy, std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const;

    void RemoveDocument(int document_id);

    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);

    void RemoveDocument(const std::execution::parallel_policy&, int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::set<std::string, std::less<>> words_;

    bool IsStopWord(const std::string_view word) const;

    static bool IsValidWord(const std::string_view word);

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(const std::string_view text) const;

    struct Query {
        std::unordered_set<std::string_view> plus_words;
        std::unordered_set<std::string_view> minus_words;
    };

    Query ParseQuery(const std::string_view raw_query) const;
    Query ParseQueryPar(const std::string_view raw_query) const;

    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    template <typename KeyMapper>
    std::vector<Document> FindAllDocuments(const Query& query, KeyMapper key_mapper) const {
        return FindAllDocuments(std::execution::seq, query, key_mapper);
    }

    template <typename KeyMapper>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&, Query query,
        KeyMapper key_mapper) const;

    template <typename KeyMapper>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, Query query,
        KeyMapper key_mapper) const;
};

template <typename KeyMapper>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy&, Query query,
    KeyMapper key_mapper) const {
    std::map<int, double> document_to_relevance;
    for (std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            if (key_mapper(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    matched_documents.reserve(document_to_relevance.size());
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({
            document_id,
            relevance,
            documents_.at(document_id).rating
            });
    }
    return matched_documents;
}

template <typename KeyMapper>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy&, Query query,
    KeyMapper key_mapper) const {

    ConcurrentMap<int, double> document_to_relevance_mt(100);

    for_each(std::execution::par,
        query.plus_words.begin(), query.plus_words.end(),
        [&document_to_relevance_mt, this
        // , &predicate
        , &query](std::string_view word)
        {
            auto contain_minus = std::any_of(std::execution::par,
                query.minus_words.begin(), query.minus_words.end(),
                [&query, &word](const auto& minus_word)
                {
                    return minus_word == word;
                });

            if (word_to_document_freqs_.count(word) && !contain_minus) {
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                std::for_each(std::execution::par,
                    word_to_document_freqs_.at(std::string(word)).begin(), word_to_document_freqs_.at(std::string(word)).end(),
                    [this, &document_to_relevance_mt, &inverse_document_freq, &query](const auto& doc_freq)
                    {
                        document_to_relevance_mt[doc_freq.first].ref_to_value += doc_freq.second * inverse_document_freq;
                    });
            }
        });

    std::atomic_int size = 0;
    const std::map<int, double>& ord_map = document_to_relevance_mt.BuildOrdinaryMap();
    std::vector<Document> matched_documents(ord_map.size());

    std::for_each(std::execution::par,
        ord_map.begin(), ord_map.end(),
        [&matched_documents, this, &size](const auto& map)
        {
            int document_id = map.first;
            double relevance = map.second;
            matched_documents[size++] = { document_id, relevance, documents_.at(document_id).rating };
        });

    matched_documents.resize(size);

    return matched_documents;
}

template <typename KeyMapper>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view query, KeyMapper key_mapper) const {

    Query structuredQuery = ParseQuery(query);
    auto matched_documents = FindAllDocuments(structuredQuery, key_mapper);
    sort(matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });
    if (matched_documents.size() > unsigned(MAX_RESULT_DOCUMENT_COUNT)) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename KeyMapper>
std::vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy, const std::string_view query, KeyMapper key_mapper) const {

    Query structuredQuery = ParseQuery(query);
    auto matched_documents = FindAllDocuments(std::execution::par, structuredQuery, key_mapper);

    sort(std::execution::par, matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });

    if (matched_documents.size() > unsigned(MAX_RESULT_DOCUMENT_COUNT)) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

