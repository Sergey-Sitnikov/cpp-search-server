#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "string_processing.h"

using namespace std;

vector<string> SplitIntoWords(const string text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

std::vector<std::string_view> SplitIntoWordsView(std::string_view text) {
    std::vector<std::string_view> words;
    std::string_view word = text;
    size_t startNewWord = 0;
    for (size_t i = 0; i < text.size(); i++) {

        if (text[i] == ' ') {
            word.remove_suffix(text.size() - i);
            if (i != startNewWord)
            {
                words.push_back(word);
            }
            startNewWord = i + 1;
            word = text;
            word.remove_prefix(startNewWord);

        }
    }
    if (!word.empty() && word.at(0) != ' ') {
        words.push_back(word);
    }
    return words;
}