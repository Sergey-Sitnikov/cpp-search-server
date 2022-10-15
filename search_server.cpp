#include "search_server.h"

#include <numeric>

void SearchServer::AddDocument(int document_id, const string_view document, DocumentStatus status, const vector<int>& ratings) {
    if (!IsValidWord(document)) {
        throw invalid_argument("Document contains special symbols"s);
    }
    else if (document_id < 0 || documents_.count(document_id)) {
        throw invalid_argument("Document_id is negative or already exist"s);
    }

    const auto words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();

    for (const auto& word : words) {
        auto [a, b] = words_.emplace(word);
        string_view word_view = *a;
        word_to_document_freqs_[word_view][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word_view] += inv_word_count;

    }
    documents_.emplace(document_id,
        DocumentData{
            ComputeAverageRating(ratings),
            status
        });
    document_ids_.insert(document_id);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string_view, double> empty_words = {};

    if (document_to_word_freqs_.count(document_id)) {
        return document_to_word_freqs_.at(document_id);
    }
    else {
        return empty_words;
    }
}

bool SearchServer::IsStopWord(const string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(const string_view text) const {
    vector<string_view> words;
    for (const string_view word : SplitIntoWordsView(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word  is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {

    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    return {
        text,
        is_minus,
        IsStopWord(text)
    };
}

SearchServer::Query SearchServer::ParseQueryPar(const string_view raw_query) const {
    Query result;
    for (std::string_view word : SplitIntoWordsView(raw_query)) {
        QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            query_word.is_minus ? result.minus_words.insert(query_word.data) : result.plus_words.insert(query_word.data);

        }
    }
    return result;
}

SearchServer::Query SearchServer::ParseQuery(const string_view raw_query) const {

    if (!IsValidWord(raw_query)) {
        throw invalid_argument("Query contains special symbols"s);
    }
    else if (raw_query.find("--"s) != string::npos) {
        throw invalid_argument("Query contains double-minus"s);
    }
    else if (raw_query.find("- "s) != string::npos) {
        throw invalid_argument("No word after '-' symbol"s);
    }
    else if (raw_query[size(raw_query) - 1] == '-') {
        throw invalid_argument("No word after '-' symbol"s);
    }

    Query query;
    for (const auto word : SplitIntoWordsView(raw_query)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            }
            else {
                query.plus_words.insert(query_word.data);
            }
        }
    }

    return query;
}

double  SearchServer::ComputeWordInverseDocumentFreq(const string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

void SearchServer::RemoveDocument(int document_id) {
    for (auto [word, freq] : document_to_word_freqs_.at(document_id)) {
        word_to_document_freqs_.at(word).erase(document_id);
    }
    document_to_word_freqs_.erase(document_id);
    document_ids_.erase(document_id);
    documents_.erase(document_id);
}

void SearchServer::RemoveDocument(const execution::sequenced_policy&, int document_id) {
    for_each(execution::seq, document_to_word_freqs_.at(document_id).begin(), document_to_word_freqs_.at(document_id).end(),
        [&, document_id](auto& el) { word_to_document_freqs_.at(el.first).erase(document_id); });
    document_to_word_freqs_.erase(document_id);
    document_ids_.erase(document_id);
    documents_.erase(document_id);
}

void SearchServer::RemoveDocument(const execution::parallel_policy&, int document_id) {
    if (documents_.count(document_id) == 0) {
        return;
    }
    std::map<std::string_view, double>& id_to_word = document_to_word_freqs_.at(document_id);
    std::vector<const string_view*> words_for_erase(id_to_word.size());
    std::transform(
        std::execution::par,
        id_to_word.begin(), id_to_word.end(), words_for_erase.begin(),
        [](const auto& word) {
            return &(word.first);
        }
    );
    std::for_each(
        std::execution::par,
        words_for_erase.begin(), words_for_erase.end(),
        [&](const auto& word) {
            word_to_document_freqs_[*word].erase(document_id);
        }
    );
    documents_.erase(document_id);
    document_ids_.erase(document_id);
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(execution::parallel_policy,
    const string_view raw_query, int document_id) const {
    if (document_to_word_freqs_.count(document_id)) {
        const Query query = ParseQueryPar(raw_query);
        const map<std::string_view, double>& word_freqs = document_to_word_freqs_.at(document_id);
        if (std::any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(), [&](auto& word) {
            return word_freqs.count(word);
            })) {
            return { vector<std::string_view>{}, documents_.at(document_id).status };
        }
        vector<std::string_view> matched_words(query.plus_words.size(), ""s);
        copy_if(std::execution::par, query.plus_words.begin(), query.plus_words.end(),
            matched_words.begin(),
            [&word_freqs](auto& word) {
                return word_freqs.find(word) != word_freqs.end();
            });
        sort(std::execution::par, matched_words.begin(), matched_words.end());
        auto it = upper_bound(matched_words.begin(), matched_words.end(), ""s);
        matched_words.erase(matched_words.begin(), it);
        return { matched_words, documents_.at(document_id).status };
    }
    return { vector<std::string_view>{}, documents_.at(document_id).status };
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy, std::string_view raw_query, int document_id) const
{
    return MatchDocument(raw_query, document_id);
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);
    std::vector<std::string_view> matched_words;
    if (documents_.count(document_id) == 0)
    {
        throw std::out_of_range("Document out of range");
    }
    bool isMinus = false;
    for (const std::string_view word : query.minus_words)
    {
        if (word_to_document_freqs_.count(word) > 0 &&
            word_to_document_freqs_.at(word).count(document_id) > 0)
        {
            matched_words.clear();
            isMinus = true;
            break;
        }
    }
    if (!isMinus)
    {
        for (const std::string_view word : query.plus_words)
        {
            if (word_to_document_freqs_.count(word) > 0 &&
                word_to_document_freqs_.at(word).count(document_id) > 0)
            {
                matched_words.push_back(word);
            }
        }
    }
    return { matched_words, documents_.at(document_id).status };
}
