
#include <sstream>
#include <algorithm>

#include "data/vocab.h"
#include "common/utils.h"
#include "common/file_stream.h"
#include "3rd_party/exception.h"
#include "3rd_party/yaml-cpp/yaml.h"

Vocab::Vocab() {
}

size_t Vocab::operator[](const std::string& word) const {
    auto it = str2id_.find(word);
    if(it != str2id_.end())
        return it->second;
    else
        return 1;
}

Words Vocab::operator()(const std::vector<std::string>& lineTokens, bool addEOS) const {
  Words words(lineTokens.size());
  std::transform(lineTokens.begin(), lineTokens.end(), words.begin(),
                  [&](const std::string& w) { return (*this)[w]; });
  if(addEOS)
    words.push_back(EOS);
  return words;
}

Words Vocab::operator()(const std::string& line, bool addEOS) const {
  std::vector<std::string> lineTokens;
  Split(line, lineTokens, " ");
  return (*this)(lineTokens, addEOS);
}

std::vector<std::string> Vocab::operator()(const Words& sentence, bool ignoreEOS) const {
  std::vector<std::string> decoded;
  for(size_t i = 0; i < sentence.size(); ++i) {
    if(sentence[i] != EOS || !ignoreEOS) {
      decoded.push_back((*this)[sentence[i]]);
    }
  }
  return decoded;
}


const std::string& Vocab::operator[](size_t id) const {
  UTIL_THROW_IF2(id >= id2str_.size(), "Unknown word id: " << id);
  return id2str_[id];
}

size_t Vocab::size() const {
  return id2str_.size();
}

void Vocab::load(const std::string& path, int max)
{
  YAML::Node vocab = YAML::Load(InputFileStream(path));
  for(auto&& pair : vocab) {
    auto str = pair.first.as<std::string>();
    auto id = pair.second.as<Word>();
    if (id < (Word)max) {
      str2id_[str] = id;
      if(id >= id2str_.size())
        id2str_.resize(id + 1);
      id2str_[id] = str;
    }
  }
  UTIL_THROW_IF2(id2str_.empty(), "Empty vocabulary " << path);
  id2str_[0] = "</s>";

}

class Vocab::VocabFreqOrderer
{
public:
  bool operator()(const Vocab::Str2Id::value_type* a, const Vocab::Str2Id::value_type* b) const {
    return a->second < b->second;
  }
};

void Vocab::create(const std::string& vocabPath, int max, const std::string& trainPath)
{
  UTIL_THROW_IF2(boost::filesystem::exists(vocabPath),
                 "Vocab file " << vocabPath << " exist. Not overwriting");

  //std::cerr << "Vocab::create" << std::endl;
  InputFileStream trainStrm(trainPath);

  // create freqency list, reuse Str2Id but use Id to store freq
  Str2Id vocab;
  std::string line;
  while (getline((std::istream&)trainStrm, line)) {
    //std::cerr << "line=" << line << std::endl;

    std::vector<std::string> toks;
    Split(line, toks);

    for (const std::string &tok: toks) {
      Str2Id::iterator iter = vocab.find(tok);
      if (iter == vocab.end()) {
        //std::cerr << "tok=" << tok << std::endl;
        vocab[tok] = 1;
      }
      else {
        //std::cerr << "tok=" << tok << std::endl;
        size_t &count = iter->second;
        ++count;
      }
    }
  }

  // put into vector & sort
  std::vector<const Str2Id::value_type*> vocabVec;
  vocabVec.reserve(max);

  for (const Str2Id::value_type &p: vocab) {
    //std::cerr << p.first << "=" << p.second << std::endl;
    vocabVec.push_back(&p);
  }
  std::sort(vocabVec.rbegin(), vocabVec.rend(), VocabFreqOrderer());

  // put into class variables
  // AND write to file
  size_t vocabSize = std::min((size_t) max, vocab.size());
  id2str_.resize(vocabSize);

  OutputFileStream vocabStrm(vocabPath);

  vocabStrm << "{\n" 
	    << "\"eos\": 0,\n"
	    << "\"UNK\": 1,\n";
  id2str_.push_back("eos");
  id2str_.push_back("UNK");
  str2id_["eos"] = 0;
  str2id_["UNK"] = 1;

  for (size_t i = 0; i < vocabSize; ++i) {
    const Str2Id::value_type *p = vocabVec[i];
    //std::cerr << p->first << "=" << p->second << std::endl;
    const std::string &str = p->first;
    str2id_[str] = i;
    id2str_.push_back(str);

    vocabStrm << "\"" << str << "\": " << (i + 2);
    if (i < vocabSize - 1) {
      vocabStrm << ",";
    }
    (std::ostream&) vocabStrm << std::endl;
  }

  (std::ostream&) vocabStrm << "}" << std::endl;
}


