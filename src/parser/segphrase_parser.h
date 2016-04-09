#ifndef __SEG_PHRASE_PARSER_H__
#define __SEG_PHRASE_PARSER_H__

#include "tools/helper.h"

#include <unordered_map>
using namespace std;

class SegPhraseParser
{
private:
    static const double INF;

// need to load
    double penalty;
    unordered_map<string, double> prob;

// generated
    int maxLen;

    void loadRankList(string filename, int topK)
    {
        FILE* in = tryOpen(filename, "r");
        vector<pair<double, string>> order;
        while (getLine(in)) {
            vector<string> tokens = splitBy(line, ',');
            string word = tokens[0];
            double score;
            fromString(tokens[1], score);
            for (size_t i = 0; i < word.size(); ++ i) {
                if (word[i] == '_') {
                    word[i] = ' ';
                }
            }
            order.push_back(make_pair(score, word));
        }
        sort(order.rbegin(), order.rend());
        
        if (topK < 0) {
            topK = order.size();
        }
        if (topK< int(order.size())) {
            order.resize(topK);
        }
        dict.clear();
        FOR (pair, order) {
            dict.insert(pair->second);
        }
    }

    void loadPhraseProb(FILE* in, size_t topK) {
        vector<string> phrases;
        size_t size;
        fread(&size, sizeof(size), 1, in);
        phrases.resize(size);

        for (size_t i = 0; i < size; ++ i) {
            Binary::read(in, phrases[i]);
        }

        vector<double> probability;
        probability.resize(size);
        if (size > 0) {
            fread(&probability[0], sizeof(probability[0]), size, in);
        }

        if (topK > 0) {
            size = min(size, topK);
        }

        vector< pair<double, size_t> > order;
        for (size_t i = 0; i < size; ++ i) {
            order.push_back(make_pair(probability[i], i));
        }

        sort(order.rbegin(), order.rend());

        for (size_t iter = 0; iter < size; ++ iter) {
            size_t i = order[iter].second;
            prob[phrases[i]] = log(probability[i]);
        }
    }

    void loadModel(string filename, size_t topK) {
        FILE* in = tryOpen(filename, "rb");

        fread(&penalty, sizeof(penalty), 1, in);

        // cerr << "[Model] penalty = " << penalty << endl;

        prob.clear();
        // unigrams
        loadPhraseProb(in, 0);

        // cerr << "[Model] # unigrams = " << prob.size() << endl;

        // real phrases
        loadPhraseProb(in, topK);

        // cerr << "[Model] # total phrases = " << prob.size() << endl;

        maxLen = 0;
        double logPenalty = log(penalty);
        FOR (pairs, prob) {
            int parts = 1;
            FOR (ch, pairs->first) {
                if ((*ch) == ' ') {
                    ++ parts;
                }
            }
            pairs->second -= (parts - 1) * logPenalty;
            maxLen = max(maxLen, parts + 1);
        }
    }

public:
    SegPhraseParser(string modelFilename, int topK = 0) {
        loadModel(modelFilename, topK);
    }

    SegPhraseParser(string modelFilename, string concept_file_path, int topK = 0) {
        loadModel(modelFilename, 0);
        dict.clear();
        loadRankList(concept_file_path, topK);
    }
    
    unordered_set<string> dict;

    void setDict(const unordered_set<string> &x) {
        dict = x;
    }

    vector<string> segment(const string &sentence) {
        string new_sentence;
        for (auto ch: sentence) {
            if(ch <= 'Z' && ch >= 'A') {
                new_sentence += ch - ('Z'-'z');
            } else if ((ch <= 'z' && ch >='a') || ch == ' ') {
                new_sentence += ch;
            } else {
                new_sentence += ' ';
            }
        }
        vector<string> tokens = splitBy(new_sentence, ' ');

        vector<double> f(tokens.size() + 1, -INF);
        vector<int> pre(tokens.size() + 1, -1);
        f[0] = 0;
        pre[0] = 0;
        double penaltyForUnrecognizedUnigram = -1e50 / tokens.size();
        for (size_t i = 0 ; i < tokens.size(); ++ i) {
            if (f[i] < -1e80) {
                continue;
            }
            string token = "";
            size_t j = i;
            while (j < tokens.size()) {
                if (j == i) {
                    token = tokens[i];
                } else {
                    token += " ";
                    token += tokens[j];
                }
                if (prob.count(token) && (dict.size() == 0 || dict.count(token))) {
                    double p = prob[token];
                    if (f[i] + p > f[j + 1]) {
                        f[j + 1] = f[i] + p;
                        pre[j + 1] = i;
                    }
                } else {
                    if (i == j) {
                        double p = penaltyForUnrecognizedUnigram;
                        if (f[i] + p > f[j + 1]) {
                            f[j + 1] = f[i] + p;
                            pre[j + 1] = i;
                        }
                    }
                    if (j > maxLen + i) {
                        break;
                    }
                }
                ++ j;
            }
        }
        if (true) {
            // get the segmentation plan
            int i = (int)tokens.size();
            vector<string> segments;
            while (i > 0) {
                int j = pre[i];
                string token = "";
                for (int k = j; k < i; ++ k) {
                    if (k > j) {
                        token += " ";
                    }
                    token += tokens[k];
                }
                i = j;
                segments.push_back(token);
            }
            reverse(segments.begin(), segments.end());
            return segments;
        }
    }
};

const double SegPhraseParser::INF = 1e100;

#endif