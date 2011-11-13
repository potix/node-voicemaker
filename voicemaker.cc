#include <sys/types.h>
#include <sys/stat.h>
#include <regex.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <list>
#include <v8.h>
#include <node.h>
#include <AquesTalk2.h>
#include <mecab.h>

using namespace v8;
using namespace node;
using namespace MeCab;
using namespace std;

namespace {

class WordPair {
public:
    const static int WORD_MAX_LENGTH = 512;
    int Get(char **src, int *srcLen, char **dst, int *dstLen);
    int GetSrc(char **src, int *srcLen);
    int Set(const char *src, int srcLen, const char *dst, int dstLen);

    WordPair();
    ~WordPair();
private:
    char *src;
    int srcLen;
    char *dst;
    int dstLen;
};

WordPair::WordPair() {
    src = NULL;
    srcLen = 0;
    dst = NULL;
    dstLen = 0;
}

WordPair::~WordPair() {
    free(src);
    free(dst);
}

int WordPair::Get(char **src, int *srcLen, char **dst, int *dstLen) {
    if (src == NULL ||
        srcLen == 0 ||
        dst == NULL ||
        dstLen == 0) {
        return 1;
    }
    *src = this->src;
    *srcLen = this->srcLen;
    *dst = this->dst;
    *dstLen = this->dstLen;

    return 0;
}

int WordPair::GetSrc(char **src, int *srcLen) {
    if (src == NULL ||
        srcLen == 0) {
        return 1;
    }
    *src = this->src;
    *srcLen = this->srcLen;

    return 0;
}

int WordPair::Set(const char *src, int srcLen, const char *dst, int dstLen) {
    char *tmpSrc = NULL;
    char *tmpDst = NULL;
    if (src == NULL ||
        srcLen == 0 ||
        dst == NULL ||
        dstLen == 0) {
        return 1;
    }
    tmpSrc = (char *)malloc(srcLen + 1);
    if (tmpSrc == NULL) {
        return 1;
    }
    tmpDst = (char *)malloc(dstLen + 1);
    if (tmpDst == NULL)  {
        free(tmpSrc);
        return 1;
    }
    memcpy(tmpSrc, src, srcLen);
    tmpSrc[srcLen] = '\0';
    memcpy(tmpDst, dst, dstLen);
    tmpDst[dstLen] = '\0';
    this->src = tmpSrc;
    this->srcLen = srcLen;
    this->dst = tmpDst;
    this->dstLen = dstLen;

    return 0;
}

class Dictionary  {
public:
    const static int PREFERRED = 1;
    const static int FILTER = 2;
    int SetDictionaryPath(const char *preferredDictionaryPath, const char *filterDictionary);
    int LoadDictionary();
    int SaveDictionary();
    // preferred dictionary or filter dictionary
    int AddWordPair(const char *src, int srcLen, const char *dst, int dstLen, int dictType);
    // preferred dictionary or filter dictionary
    int DelWordPair(const char *src, int srcLen, int dictType);
    // preferred dictionary only
    int GetDstWord(const char *src, int srcLen, char **dst, int *dstLen);
    // filter dictionary only
    int GetWordPairBegin(list<WordPair *>::iterator *wordPairIterator);
    int GetWordPairNext(char **src, int *srcLen, char **dst, int *dstLen, list<WordPair *>::iterator *wordPairIterator);
    // preferred dictionary or filter dictionary
    int GetExtensionRatio(int *ratio, int dictType);


    Dictionary();
    ~Dictionary();
private:
    int hashSize;
    char *preferredDictionaryPath;
    char *filterDictionaryPath;
    list<WordPair *> *preferredDictionary;
    list<WordPair *> *filterDictionary;
    int preferredExtensionRatio;
    int filterExtensionRatio;
 
    int GetHashValue(const char *key, int keyLen);
    int ClearDictionary(int dictType);
};

Dictionary::Dictionary() {
    hashSize = 5003;
    preferredDictionaryPath = NULL;
    filterDictionaryPath = NULL;
    preferredExtensionRatio = 2;
    filterExtensionRatio = 2;
    preferredDictionary = new list<WordPair *>[hashSize];
    filterDictionary = new list<WordPair *>;
}

Dictionary::~Dictionary() {
    int i;
    // preferred dictionary
    for (i = 0; i < hashSize; i++) {
        list<WordPair *>::iterator wordPairIterator = preferredDictionary[i].begin();
        while (wordPairIterator != preferredDictionary[i].end()) {
             WordPair *wordPair = *wordPairIterator;
             wordPairIterator = preferredDictionary[i].erase(wordPairIterator);
             delete wordPair;
        }
    }
    delete [] preferredDictionary;
    free(preferredDictionaryPath);
    // filter dictionary
    list<WordPair *>::iterator wordPairIterator = filterDictionary->begin();
    while (wordPairIterator != filterDictionary->end()) {
         WordPair *wordPair = *wordPairIterator;
         wordPairIterator = filterDictionary->erase(wordPairIterator);
         delete wordPair;
    }
    delete filterDictionary;
    free(filterDictionaryPath);
}

int Dictionary::GetHashValue(const char *key, int keyLen) {
    unsigned int v = keyLen;

    for (int i = 0; i < keyLen; i++) {
       v = v * 31 + key[i];
    }
    return (int)(v % hashSize);
}

int Dictionary::ClearDictionary(int dictType) {
    int i;
    if (dictType != PREFERRED && dictType != FILTER) {
        return 1;
    }
    if (dictType == PREFERRED) {
        for (i = 0; i < hashSize; i++) {
            list<WordPair *>::iterator wordPairIterator = preferredDictionary[i].begin();
            while (wordPairIterator != preferredDictionary[i].end()) {
                WordPair *wordPair = *wordPairIterator;
                wordPairIterator = preferredDictionary[i].erase(wordPairIterator);
                delete wordPair;
            }
        }
    }
    if (dictType == FILTER) {
        list<WordPair *>::iterator wordPairIterator = filterDictionary->begin();
        while (wordPairIterator != filterDictionary->end()) {
            WordPair *wordPair = *wordPairIterator;
            wordPairIterator = filterDictionary->erase(wordPairIterator);
            delete wordPair;
        }
    }

    return 0;
}

int Dictionary::LoadDictionary() {
    int error = 0;
    FILE *fp;
    char line[(WordPair::WORD_MAX_LENGTH * 2) + 2];
    const char *preferredPath = "./voicemaker_preferred.dict";
    const char *filterPath = "./voicemaker_filter.dict";
    int dictTypes[] = { PREFERRED, FILTER };
    int i;

    if (preferredDictionaryPath) {
        preferredPath = preferredDictionaryPath;
    } 
    if (filterDictionaryPath) {
        filterPath = filterDictionaryPath;
    } 
    for (i = 0; i < sizeof(dictTypes)/sizeof(dictTypes[0]) && !error; i++) {
        if (ClearDictionary(dictTypes[i])) {
            return 1;
        }
        if (dictTypes[i] == PREFERRED) {
            fp = fopen(preferredPath, "r");
        } else if (dictTypes[i] == FILTER) {
            fp = fopen(filterPath, "r");
        }
        if (fp == NULL) {
            return 2;
        }
        while(fgets(line, sizeof(line), fp)) { 
            char *srcStartPtr = NULL;
            char *dstStartPtr = NULL;
            char *endPtr = NULL;
            if (*line == '\0') {
                continue;
            }
            srcStartPtr = line;
            dstStartPtr = strchr(line, ' ');
            if (!dstStartPtr) {
                continue;
            }
            *dstStartPtr = '\0';
            dstStartPtr++;
            endPtr = strchr(dstStartPtr, '\n');
            if (endPtr) {
                *endPtr = '\0';
            }
            if (*srcStartPtr == '\0' || *dstStartPtr == '\0') {
                continue;
            }
            if (AddWordPair(srcStartPtr, strlen(srcStartPtr), dstStartPtr, strlen(dstStartPtr), dictTypes[i])) {
                error = 3;
            }
        }
        fclose(fp);
    }

    return error;
}

int Dictionary::SaveDictionary() {
    int error = 0;
    FILE *fp;
    const char *preferredPath = "./voicemaker_preferred.dict";
    const char *filterPath = "./voicemaker_filter.dict";

    if (preferredDictionaryPath) {
        preferredPath = preferredDictionaryPath;
    } 
    if (filterDictionaryPath) {
        filterPath = filterDictionaryPath;
    } 
    // preferred dictionary
    fp = fopen(preferredPath, "w+");
    if (fp == NULL) {
        return 1;
    }
    for (int i = 0; i < hashSize; i++) {
        list<WordPair *>::iterator wordPairIterator = preferredDictionary[i].begin();
        while (wordPairIterator != preferredDictionary[i].end()) {
            char *dicSrc;
            int dicSrcLen;
            char *dicDst;
            int dicDstLen;
            WordPair *wordPair = *wordPairIterator;
            if (wordPair->Get(&dicSrc, &dicSrcLen, &dicDst, &dicDstLen)) {
                error = 2;
            }
            fwrite(dicSrc, (size_t)dicSrcLen, 1, fp);
            fwrite(" ", 1, 1, fp);
            fwrite(dicDst,(size_t) dicDstLen, 1, fp);
            fwrite("\n", 1, 1, fp);
            wordPairIterator++;
        }
    }
    fclose(fp);

    // filter dictionary
    fp = fopen(filterPath, "w+");
    if (fp == NULL) {
        return 1;
    }
    list<WordPair *>::iterator wordPairIterator = filterDictionary->begin();
    while (wordPairIterator != filterDictionary->end()) {
        char *dicSrc;
        int dicSrcLen;
        char *dicDst;
        int dicDstLen;
        WordPair *wordPair = *wordPairIterator;
        if (wordPair->Get(&dicSrc, &dicSrcLen, &dicDst, &dicDstLen)) {
            error = 2;
        }
        fwrite(dicSrc, (size_t)dicSrcLen, 1, fp);
        fwrite(" ", 1, 1, fp);
        fwrite(dicDst,(size_t) dicDstLen, 1, fp);
        fwrite("\n", 1, 1, fp);
        wordPairIterator++;
    }
    fclose(fp);

    return error;
}

int Dictionary::SetDictionaryPath(const char* preferredDictionaryPath, const char* filterDictionaryPath) {
    if (preferredDictionaryPath == NULL ||
        filterDictionary == NULL) {
        return 1;
    }
    free(this->preferredDictionaryPath);
    this->preferredDictionaryPath = strdup(preferredDictionaryPath);
    if (this->preferredDictionaryPath == NULL) {
        return 2;
    }
    free(this->filterDictionaryPath);
    this->filterDictionaryPath = strdup(filterDictionaryPath);
    if (this->filterDictionaryPath == NULL) {
        return 3;
    }

    return 0;
}

int Dictionary::AddWordPair(const char *src, int srcLen, const char *dst, int dstLen, int dictType) {
    if (src == NULL ||
        srcLen <= 0 ||
        dst == NULL ||
        dstLen <= 0 ||
        (dictType != PREFERRED && dictType != FILTER)) {
        return 1;
    }
    if (dictType == PREFERRED) {
        int hashValue = GetHashValue(src, srcLen);
        WordPair *wordPair = new WordPair();
        preferredDictionary[hashValue].push_front(wordPair);
        if (wordPair->Set(src, srcLen, dst, dstLen)) {
            return 1;
        }
        if (dstLen / srcLen > preferredExtensionRatio) {
            preferredExtensionRatio = (dstLen / srcLen) + 1;
        }
    } else if (dictType == FILTER) {
        WordPair *wordPair = new WordPair();
        filterDictionary->push_back(wordPair);
        if (wordPair->Set(src, srcLen, dst, dstLen)) {
            return 1;
        }
        if (dstLen / srcLen > filterExtensionRatio) {
            filterExtensionRatio = (dstLen / srcLen) + 1;
        }
    }

    return 0;
}

int Dictionary::DelWordPair(const char *src, int srcLen, int dictType) {
    if (src == NULL ||
        srcLen <= 0 ||
        (dictType != PREFERRED && dictType != FILTER)) {
        return 1;
    }
    if (dictType == PREFERRED) {
        int hashValue = GetHashValue(src, srcLen);
        list<WordPair *>::iterator wordPairIterator = preferredDictionary[hashValue].begin();
        while (wordPairIterator != preferredDictionary[hashValue].end()) {
             char *dicSrc;
             int dicSrcLen;
             WordPair *wordPair = *wordPairIterator;
             if (wordPair->GetSrc(&dicSrc, &dicSrcLen)) {
                 return 1;
             }
             if (dicSrcLen == srcLen && strncmp(dicSrc, src, srcLen) == 0) {
                 wordPairIterator = preferredDictionary[hashValue].erase(wordPairIterator);
                 delete wordPair;
             } else {
                 wordPairIterator++;
             }
        }
    } else if (dictType == FILTER) {
        list<WordPair *>::iterator wordPairIterator = filterDictionary->begin();
        while (wordPairIterator != filterDictionary->end()) {
             char *dicSrc;
             int dicSrcLen;
             WordPair *wordPair = *wordPairIterator;
             if (wordPair->GetSrc(&dicSrc, &dicSrcLen)) {
                 return 1;
             }
             if (dicSrcLen == srcLen && strncmp(dicSrc, src, srcLen) == 0) {
                 wordPairIterator = filterDictionary->erase(wordPairIterator);
                 delete wordPair;
             } else {
                 wordPairIterator++;
             }
        }
    }

    return 0;
}

int Dictionary::GetDstWord(const char *src, int srcLen, char **dst, int *dstLen) {
    if (src == NULL ||
        srcLen <= 0 ||
        dst == NULL ||
        dstLen == NULL) {
        return 1;
    }
    char copySrc[WordPair::WORD_MAX_LENGTH];
    strncpy(copySrc, src, sizeof(copySrc));
    copySrc[sizeof(copySrc) - 1] = '\0';
    int hashValue = GetHashValue(copySrc, srcLen);
    list<WordPair *>::iterator wordPairIterator = preferredDictionary[hashValue].begin();
    while (wordPairIterator != preferredDictionary[hashValue].end()) {
         char *dicSrc;
         int dicSrcLen;
         char *dicDst;
         int dicDstLen;
         WordPair *wordPair = *wordPairIterator;
         wordPair->Get(&dicSrc, &dicSrcLen, &dicDst, &dicDstLen);
         if (dicSrcLen == srcLen && strncmp(dicSrc, copySrc, srcLen) == 0) {
              *dst = dicDst;
              *dstLen = dicDstLen;
              return 0;
         }
         wordPairIterator++; 
    }

    return 1;
}

int Dictionary::GetWordPairBegin(list<WordPair *>::iterator *wordPairIterator) {
    if (wordPairIterator == NULL) {
        return 1;
    }
    *wordPairIterator = filterDictionary->begin();

    return 0;
}

int Dictionary::GetWordPairNext(char **src, int *srcLen, char **dst, int *dstLen, list<WordPair *>::iterator *wordPairIterator) {
    if (src == NULL ||
        srcLen <= 0 ||
        dst == NULL ||
        dstLen == NULL ||
        wordPairIterator == NULL) {
        return 1;
    }
    if ((*wordPairIterator) == filterDictionary->end()) {
        *src = NULL;
        *srcLen = 0;
        *dst = NULL;
        *dstLen = 0;
        return -1;
    }
    WordPair *wordPair = *(*wordPairIterator);
    wordPair->Get(src, srcLen, dst, dstLen);
    (*wordPairIterator)++; 

    return 0;
}

int Dictionary::GetExtensionRatio(int *ratio, int dictType) {
    if (dictType != PREFERRED && dictType != FILTER) {
        return 1;
    }
    if (dictType == PREFERRED) {
        *ratio = preferredExtensionRatio;
    } else if (dictType == FILTER) {
        *ratio = filterExtensionRatio;
    }

    return 0;
}

class VoiceMaker: ObjectWrap {
public:
    static void Initialize(const Handle<Object>& target);
    static Handle<Value> New(const Arguments& args);
    static Handle<Value> Convert(const Arguments& args);
    static Handle<Value> GetErrorText(const Arguments& args);
    static Handle<Value> SetDictionary(const Arguments& args);
    static Handle<Value> LoadDictionary(const Arguments& args);
    static Handle<Value> SaveDictionary(const Arguments& args);
    static Handle<Value> AddWord(const Arguments& args, int dictType);
    static Handle<Value> AddPreferredWord(const Arguments& args);
    static Handle<Value> AddFilterWord(const Arguments& args);
    static Handle<Value> DelWord(const Arguments& args, int dictType);
    static Handle<Value> DelPreferredWord(const Arguments& args);
    static Handle<Value> DelFilterWord(const Arguments& args);

    VoiceMaker();
    ~VoiceMaker();

private:

    char *errorText;
    const char *base64char;
    Dictionary *dictionary;
 
    void ConvertFree(char *preText, char *newText, mecab_t *mecab, char *fixupText, char *filterFree, unsigned char *modelData, unsigned char *waveData);
    Handle<Value> Convert(const char* text, int textLength, int speed, const char *modelFile);

    void FixupFree(char *newText);
    int Fixup(char **fixupText, const char *text);

    void FilterFree(char *newText);
    int Filter(char **filterText, const char *text);

    void LoadFileFree(unsigned char *fileData);
    int LoadFile(const char *filePath, unsigned char **fileData, size_t *fileSize);

    void Base64EncodeFree(char *out);
    int Base64Encode(char **out, int *outLen, const unsigned char *in, int inSize);
};

VoiceMaker::VoiceMaker() {
    errorText = NULL;
    base64char = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    dictionary = new Dictionary();
}

VoiceMaker::~VoiceMaker() {
    free(errorText);
    delete dictionary;
}

void VoiceMaker::Base64EncodeFree(char *out) {
    free(out);
}

int VoiceMaker::Base64Encode(char **out, int *outLen, const unsigned char *in, int inSize) {
    char *encoded;
    const unsigned char *inp;
    char *outp;
    int inLen;

    encoded = (char *)malloc(inSize * 4 / 3 + 4);
    if (encoded == NULL) {
            return 1;
    }
    outp = encoded;
    inp = in;
    inLen = inSize;

    while (inLen >= 3) {
        *outp++ = base64char[(inp[0] >> 2) & 0x3f];
        *outp++ = base64char[((inp[0] & 0x03) << 4) | ((inp[1] >> 4) & 0x0f)];
        *outp++ = base64char[((inp[1] & 0x0f) << 2) | ((inp[2] >> 6) & 0x03)];
        *outp++ = base64char[inp[2] & 0x3f];
        inp += 3;
        inLen -= 3;
    }
    if (inLen > 0) {
        *outp++ = base64char[(inp[0] >> 2) & 0x3f];
        if (inLen == 1) {
            *outp++ = base64char[(inp[0] & 0x03) << 4];
            *outp++ = '=';
        } else {
            *outp++ = base64char[((inp[0] & 0x03) << 4) | ((inp[1] >> 4) & 0x0f)];
            *outp++ = base64char[((inp[1] & 0x0f) << 2)];
        }
        *outp++ = '=';
    }
    *outp = '\0';
    *out = encoded;
    if (outLen) {
        *outLen = outp - encoded;
    }

    return 0;
}

void VoiceMaker::LoadFileFree(unsigned char *fileData) {
    free(fileData);
}

int VoiceMaker::LoadFile(const char *filePath, unsigned char **fileData, size_t *fileSize) {
    struct stat st;
    int fd = -1;
    unsigned char *data = NULL;
    size_t size;
    ssize_t rsz;

    *fileData = NULL;
    *fileSize = 0;

    if (stat(filePath, &st) != 0) {
        return 1;
    }
    size = (size_t)st.st_size;
    data = (unsigned char*)malloc(size);
    if (data == NULL) {
        return 2;
    }
    if ((fd = open(filePath, O_RDONLY)) < 0) {
        free(data);
        return 3;
    }
    if ((rsz = read(fd, data, size)) != size) {
        close(fd);
        free(data);
        return 4;
    }
    close(fd); 
    *fileData = data;
    *fileSize = size;

    return 0;
}

void VoiceMaker::FixupFree(char *fixupText) {
    free(fixupText);
}

#define MAX_REG_MATCH 5 
int VoiceMaker::Fixup(char **fixupText, const char *text) {
    int i;
    regex_t pre;
    regmatch_t pmatch[MAX_REG_MATCH];
    char *origText = NULL;
    int origTextLength;
    char *newText = NULL;
    int newTextLength;
    int newTextCurrent;

    if (fixupText == NULL ||
        text == NULL) {
        return 1;
    }
    if (strlen(text) < 1) {
        return 2;
    }
    origText = strdup(text);
    if (origText == NULL) {
        return 3;
    }
    if (regcomp(&pre, " ([-.[:digit:]]+) (([^ ]+) )?", REG_EXTENDED | REG_ICASE)) {
        free(origText);
        return 4;
    }
    while (1) {
        char *num = "NUMK";
        int numLen = sizeof("NUMK") - 1;
        origTextLength = strlen(origText);
        newTextLength = origTextLength + 22;
        newText = (char *)malloc(newTextLength);
        if (!newText) {
            free(origText);
            regfree(&pre);
            return 5;
        }
        if(regexec(&pre, origText, sizeof(pmatch)/sizeof(pmatch[0]), pmatch, 0)) {
            strcpy(newText, origText);
            break;
        } else {
            if (pmatch[0].rm_so < 0 || pmatch[0].rm_eo < 0 ||
                pmatch[1].rm_so < 0 || pmatch[1].rm_eo < 0) {
                strcpy(newText, origText);
                break;
            }
            newTextCurrent = 0;
            memcpy(&newText[newTextCurrent], origText, pmatch[0].rm_so);
            newTextCurrent += pmatch[0].rm_so;
            for (i = 0; pmatch[1].rm_so + i < pmatch[1].rm_eo; i++) {
                 if (origText[pmatch[1].rm_so + i] == '-') {
                     num = "NUM";
                     numLen = sizeof("NUM") - 1;
                 }
            }
            memcpy(&newText[newTextCurrent], "<" , 1);
            newTextCurrent += 1;
            memcpy(&newText[newTextCurrent], num , numLen);
            newTextCurrent += numLen;
            memcpy(&newText[newTextCurrent], " VAL=" , 5);
            newTextCurrent += 5;
            int headZeroPadding = 1;
            for (i = 0; i < pmatch[1].rm_eo - pmatch[1].rm_so; i++) {
                 if (origText[pmatch[1].rm_so + i] != '0') {
                     headZeroPadding = 0;
                 }
                 if (headZeroPadding && origText[pmatch[1].rm_so + i] == '0' && origText[pmatch[1].rm_so + i + 1] == '0') {
                     continue;
                 } else {
                     memcpy(&newText[newTextCurrent], &origText[pmatch[1].rm_so + i], 1);
                     newTextCurrent += 1;
                   
                 }
            }
            if (pmatch[2].rm_so >= 0 && pmatch[2].rm_eo >= 0 ||
                pmatch[3].rm_so >= 0 && pmatch[3].rm_eo >= 0) {
                memcpy(&newText[newTextCurrent], " COUNTER=" , 9);
                newTextCurrent += 9;
                for (i = 0; i < pmatch[3].rm_eo - pmatch[3].rm_so; i++) {
                    if (!isascii(origText[pmatch[3].rm_so + i])) {
                        memcpy(&newText[newTextCurrent], &origText[pmatch[3].rm_so + i], 1);
                        newTextCurrent += 1;
                    }
                }
            }
            memcpy(&newText[newTextCurrent], ">" , 1);
            newTextCurrent += 1;
            strcpy(&newText[newTextCurrent], &origText[pmatch[0].rm_eo]);
        }
        free(origText);
        origText = newText;
    }
    free(origText);
    regfree(&pre);
    *fixupText = newText;

    return 0;
}

void VoiceMaker::FilterFree(char *filterdText) {
    free(filterdText);
}

int VoiceMaker::Filter(char **filterdText, const char *text) {
    int result;
    int i;
    char *newText, *newTextBack;
    int textLength, newTextLength;
    char *exist;
    list<WordPair *>::iterator wordPairIterator;
    char *dicSrc;
    int dicSrcLen;
    char *dicDst;
    int dicDstLen;
    int ext;
    if (filterdText == NULL ||
        text == NULL) {
        return 1;
    }
    textLength = strlen(text);
    if (textLength < 1) {
        return 2;
    }
    if (dictionary->GetExtensionRatio(&ext, Dictionary::FILTER)) {
        return 3;
    }
    newTextLength = textLength * ext;
    newText = (char *)malloc(newTextLength);
    if (!newText) {
        return 4;
    }
    newTextBack = (char *)malloc(newTextLength);
    if (!newTextBack) {
        free(newText);
        return 5;
    }
    strcpy(newText, text);
    if (dictionary->GetWordPairBegin(&wordPairIterator)) {
        return 6;
    }
    while(1) {
       result = dictionary->GetWordPairNext(&dicSrc, &dicSrcLen, &dicDst, &dicDstLen, &wordPairIterator);
       if (result == -1) {
           break;
       }
       if (result == 1) {
           free(newText);
           free(newTextBack);
           return 7;
       }
       if ((*dicSrc >= 0x30 && *dicSrc <= 0x39 && dicSrcLen == 1) ||
           (*dicSrc == ' ' && dicSrcLen == 1) ||
           (*dicSrc == '-' && dicSrcLen == 1) ||
           (*dicSrc == '.' && dicSrcLen == 1)) {
           continue;
       }
       if (textLength < dicSrcLen) {
           continue;
       }
       if (newTextLength < dicDstLen) {
           continue;
       }
       while(1) {
           exist = strcasestr(newText, dicSrc);
           if (!exist) {
               break;
           }
           strcpy(newTextBack, exist);
           strcpy(exist, dicDst);
           strcpy(exist + dicDstLen, newTextBack + dicSrcLen);
       }
    }
    free(newTextBack);
    *filterdText = newText;

    return 0;
}

void VoiceMaker::ConvertFree(char *preText, char *newText, mecab_t *mecab, char *fixupText, char *filterText, unsigned char *modelData, unsigned char *waveData) {
    free(preText);
    free(newText);
    if (mecab) {
        mecab_destroy(mecab);
    }
    if (fixupText) {
        FixupFree(fixupText);
    }
    if (filterText) {
        FixupFree(filterText);
    }
    if (modelData) {
        LoadFileFree(modelData);
    }
    if (waveData) {
        AquesTalk2_FreeWave(waveData);
    }
}

Handle<Value> VoiceMaker::Convert(const char* text, int textLength, int speed, const char *modelFile) {
    HandleScope scope;
    const char *error = NULL;
    mecab_t *mecab = NULL;
    const mecab_node_t *node;
    int argc = 1;
    char *argv[] = { "voicemaker" };
    char *newText = NULL;
    char *newTextPtr = NULL;
    char *fixupText = NULL;
    char *filterText = NULL;
    int newTextLength;
    unsigned char *modelData = NULL;
    size_t modelSize;
    unsigned char *waveData = NULL;
    int waveSize;
    char *waveBase64 = NULL;
    int waveBase64Len;
    int result;
    char *dst;
    int dstLen;
    int ext;
    char *preText = NULL;
    int preTextLen;
    int prevAlpha;
    int i;

    if (textLength < 1) {
        Local<String> dataString = String::New("");
        return scope.Close(dataString);
    }
    preText = (char *)malloc(textLength * 2);
    if (!preText) {
         return scope.Close(ThrowException(Exception::Error(String::New("failed in allocate buffer of pre text."))));
    }
    preTextLen = 0;
    prevAlpha = 1;
    for (i = 0; i < textLength; i++) {
        if (isalpha(text[i])) {
            if (text[i] != ' ' && !prevAlpha) {
                preText[preTextLen++] = ',';
            }
            preText[preTextLen++] = text[i];
            prevAlpha = 1;
        } else {
            if (text[i] != ' ' && prevAlpha) {
                preText[preTextLen++] = ',';
            }
            preText[preTextLen++] = text[i];
            prevAlpha = 0;
        }
    }
    preText[preTextLen++] = '\0';
    if (dictionary->GetExtensionRatio(&ext, Dictionary::PREFERRED)) {
        scope.Close(ThrowException(Exception::Error(String::New("failed in get extension ratio of preferred dictionary."))));
    }
    newTextLength = preTextLen * 15 * 4 * ext;
    newText = (char *)malloc(newTextLength);
    if (!newText) {
         ConvertFree(preText, newText, mecab, fixupText, filterText, modelData, waveData);
         return scope.Close(ThrowException(Exception::Error(String::New("failed in allocate buffer of new text."))));
    }
    newTextPtr = newText;
    mecab = mecab_new(argc, argv);
    if (!mecab) {
         ConvertFree(preText, newText, mecab, fixupText, filterText, modelData, waveData);
         return ThrowException(Exception::Error(String::New("failed in create instance of Mecab::Tagger.")));
    }
    node = mecab_sparse_tonode(mecab, preText);
    if (!node) {
         ConvertFree(preText, newText, mecab, fixupText, filterText, modelData, waveData);
         return ThrowException(Exception::Error(String::New("failed in create instance of Mecab::Node.")));
    }
    int digit = 0;
    int separator = 0;
    for (; node; node = node->next) {
         const char *startPtr = NULL;
         const char *endPtr = NULL;
         const char *currentPtr = node->feature;
         int delimiter = 0;
         int length;
         int counter = 0;

         if (*node->surface >= 0x30 && *node->surface <= 0x39 && node->length == 1) {
             if (separator != 0) {
                 newTextPtr -= separator;
             }
             if (digit == 0) {
                 memcpy(newTextPtr, " ", 1);
                 newTextPtr += 1;
             }
             memcpy(newTextPtr, node->surface, node->length);
             newTextPtr += node->length;
             digit += 1;
             separator = 0;
             continue;
         } else if ((*node->surface == '-' && node->length == 1) ||
                    (*node->surface == '.' && node->length == 1)) {
             if (digit != 0) {
                 memcpy(newTextPtr, node->surface, node->length);
                 newTextPtr += node->length;
                 digit += 1;
                 separator = 0;
                 continue;
             }
             digit = 0;
             separator = 0;
         } else if (*node->surface == ',' && node->length == 1) {
             if (digit != 0) {
                 memcpy(newTextPtr, node->surface, node->length);
                 newTextPtr += node->length;
                 digit += 1;
                 separator += 1;
                 continue;
             }
             digit = 0;
             separator = 0;
         } else {
             if (digit != 0) {
                 memcpy(newTextPtr, " ", 1);
                 newTextPtr += 1;
                 counter = 1;
             }
             digit = 0;
             separator = 0;
         }
          
         if (dictionary->GetDstWord(node->surface, node->length, &dst, &dstLen) == 0) {
             memcpy(newTextPtr, dst, dstLen);
             newTextPtr += dstLen;
             if (counter) {
                 memcpy(newTextPtr, " ", 1);
                 newTextPtr += 1;
             }
             continue;
         }
         while (*currentPtr != '\0') {
             if (*currentPtr == ',') {
                 delimiter++;
	         if (delimiter == 8) {
                     startPtr = currentPtr;
                     startPtr++;
                 } else if (delimiter == 9) {
                     endPtr = currentPtr;
                     break;
                 }
             }
             currentPtr++;
         }
         if (!startPtr && !endPtr && delimiter < 7) {
             startPtr = node->surface;
             length = node->length;
             memcpy(newTextPtr, startPtr, length);
             newTextPtr += length;
             if (counter) {
                 memcpy(newTextPtr, " ", 1);
                 newTextPtr += 1;
             }
         } else {
             if (startPtr && endPtr) {
                 if (*startPtr == '"' && *(endPtr - 1) == '"') {
                     startPtr += 1;
                     endPtr -= 1;
                 }
                 length = endPtr - startPtr;
                 if (length >= 0) {
                     memcpy(newTextPtr, startPtr, length);
                     newTextPtr += length;
                     if (counter) {
                         memcpy(newTextPtr, " ", 1);
                         newTextPtr += 1;
                     }
                 }
             }
         }
    }
    if (digit) {
        memcpy(newTextPtr, " ", 1);
        newTextPtr += 1;
    }
    *newTextPtr = '\0';
    mecab_destroy(mecab);
    mecab = NULL;
    free(preText);
    preText = NULL;
    if ((result = Filter(&filterText, newText))) {
        free(errorText);
        errorText = strdup(newText);
        ConvertFree(preText, newText, mecab, fixupText, filterText, modelData, waveData);
        switch (result) {
        case 1:
            error = "invalid argument in filter.";
            break;
        case 2:
            error = "too short text in filter.";
            break;
        case 3:
            error = "failed in get extension ratio of filter dictionary in filter.";
            break;
        case 4:
            error = "failed in allocate memory of new text in filter.";
            break;
        case 5:
            error = "failed in allocate memory of backup text in filter.";
            break;
        case 6:
            error = "failed in get dictionary iterator in filter.";
            break;
        case 7:
            error = "failed in get word pair in filter.";
            break;
        default:
            error = "preferred error in filter.";
            break;
        }
        return scope.Close(ThrowException(Exception::Error(String::New(error))));
    }
    free(newText);
    newText = NULL;
    if ((result = Fixup(&fixupText, filterText))) {
        free(errorText);
        errorText = strdup(filterText);
        ConvertFree(preText, newText, mecab, fixupText, filterText, modelData, waveData);
        switch (result) {
        case 1:
            error = "invalid argument in fixup.";
            break;
        case 2:
            error = "too short text in fixup.";
            break;
        case 3:
            error = "failed in allocate memory of orignal text in fixup.";
            break;
        case 4:
            error = "failed in failed in compile regex in fixup.";
            break;
        case 5:
            error = "failed in allocate memory of new text in fixup.";
            break;
        default:
            error = "preferred error in fixup.";
            break;
        }
        return scope.Close(ThrowException(Exception::Error(String::New(error))));
    }
    free(filterText);
    filterText = NULL;
    if (modelFile) {
        if ((result = LoadFile(modelFile, &modelData, &modelSize))) {
            ConvertFree(preText, newText, mecab, fixupText, filterText, modelData, waveData);
            switch (result) {
            case 1:
                error = "not found model file in model file loader.";
                break;
            case 2:
                error = "failed in allocate memory of model data in model file loader.";
                break;
            case 3:
                error = "failed in open file of model in model file loader.";
                break;
            case 4:
                error = "failed in read data of model in model file loader.";
                break;
            default:
                error = "preferred error in model file loader.";
                break;
            }
            return scope.Close(ThrowException(Exception::Error(String::New(error))));
        }
    }
    waveData = AquesTalk2_Synthe_Utf8(fixupText, speed, &waveSize, modelData);
    if (!waveData) {
        free(errorText);
        errorText = strdup(fixupText);
        ConvertFree(preText, newText, mecab, fixupText, filterText, modelData, waveData);
        return ThrowException(Exception::Error(String::New("failed in create data of wave.")));
    }
    FilterFree(fixupText);
    fixupText = NULL;
    LoadFileFree(modelData);
    modelData = NULL;
    if (Base64Encode(&waveBase64, &waveBase64Len, waveData, waveSize)) {
        ConvertFree(preText, newText, mecab, fixupText, filterText, modelData, waveData);
        return scope.Close(ThrowException(Exception::Error(String::New("failed in encode to base64."))));
    }
    AquesTalk2_FreeWave(waveData);
    Local<String> dataString = String::New(waveBase64);
    Base64EncodeFree(waveBase64);

    return scope.Close(dataString);
}

Handle<Value> VoiceMaker::SetDictionary(const Arguments& args) {
    HandleScope scope;

    if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsString()) {
        return scope.Close(ThrowException(Exception::Error(String::New("Bad arguments. no dictionary path."))));
    }
    VoiceMaker *voicemaker = Unwrap<VoiceMaker>(args.This());
    String::Utf8Value preferredDictionaryPath(args[0]->ToString());
    String::Utf8Value filterDictionaryPath(args[1]->ToString());
    if (voicemaker->dictionary->SetDictionaryPath(*preferredDictionaryPath, *filterDictionaryPath)) {
        return scope.Close(ThrowException(Exception::Error(String::New("failed in set dictionary path."))));
    }
}

Handle<Value> VoiceMaker::LoadDictionary(const Arguments& args) {
    HandleScope scope;
    int result;
    char *error;

    if (args.Length() > 0) {
        return scope.Close(ThrowException(Exception::Error(String::New("Bad arguments. must be no argument."))));
    }
    VoiceMaker *voicemaker = Unwrap<VoiceMaker>(args.This());
    if ((result = voicemaker->dictionary->LoadDictionary())) {
        switch (result) {
        case 1:
            error = "failed in clear dictionary.";
            break;
        case 2:
            error = "failed in open dictionary.";
            break;
        case 3:
            error = "failed in set word.";
            break;
        default:
            error = "preferred error";
            break;
        }
        return scope.Close(ThrowException(Exception::Error(String::New(error))));
    }
}

Handle<Value> VoiceMaker::SaveDictionary(const Arguments& args) {
    HandleScope scope;
    int result;
    char *error;

    if (args.Length() > 0) {
        return scope.Close(ThrowException(Exception::Error(String::New("Bad arguments. must be no argument."))));
    }
    VoiceMaker *voicemaker = Unwrap<VoiceMaker>(args.This());
    if ((result = voicemaker->dictionary->SaveDictionary())) {
        switch (result) {
        case 1:
            error = "failed in open dictionary.";
            break;
        case 2:
            error = "failed in get word.";
            break;
        default:
            error = "preferred error";
            break;
        }
        return scope.Close(ThrowException(Exception::Error(String::New(error))));
    }
}

Handle<Value> VoiceMaker::AddWord(const Arguments& args, int dictType) {
    HandleScope scope;

    if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsString()) {
        return scope.Close(ThrowException(Exception::Error(String::New("Bad arguments. required two string arguments."))));
    }
    VoiceMaker *voicemaker = Unwrap<VoiceMaker>(args.This());
    String::Utf8Value src(args[0]->ToString());
    String::Utf8Value dst(args[1]->ToString());
    if (src.length() < 1 || dst.length() < 1) {
        return scope.Close(ThrowException(Exception::Error(String::New("Bad arguments. specified empty string."))));
    }
    if (voicemaker->dictionary->AddWordPair(*src, src.length(), *dst, dst.length(), dictType)) {
        return scope.Close(ThrowException(Exception::Error(String::New("failed in add word."))));
    }
}

Handle<Value> VoiceMaker::AddPreferredWord(const Arguments& args) {
    return AddWord(args, Dictionary::PREFERRED);
}

Handle<Value> VoiceMaker::AddFilterWord(const Arguments& args) {
    return AddWord(args, Dictionary::FILTER);
}

Handle<Value> VoiceMaker::DelWord(const Arguments& args, int dictType) {
    HandleScope scope;

    if (args.Length() != 1 || !args[0]->IsString()) {
        return scope.Close(ThrowException(Exception::Error(String::New("Bad arguments. required string argument."))));
    }
    VoiceMaker *voicemaker = Unwrap<VoiceMaker>(args.This());
    String::Utf8Value src(args[0]->ToString());
    if (src.length() < 1) {
        return scope.Close(ThrowException(Exception::Error(String::New("Bad arguments. specified empty string."))));
    }
    if (voicemaker->dictionary->DelWordPair(*src, src.length(), dictType)) {
        return scope.Close(ThrowException(Exception::Error(String::New("failed in delete word."))));
    }
}

Handle<Value> VoiceMaker::DelPreferredWord(const Arguments& args) {
    return DelWord(args, Dictionary::PREFERRED);
}

Handle<Value> VoiceMaker::DelFilterWord(const Arguments& args) {
    return DelWord(args, Dictionary::FILTER);
}

Handle<Value> VoiceMaker::Convert(const Arguments& args) {
    HandleScope scope;
    int modelArgumentIndex = -1;
    int speed = 100;

    VoiceMaker *voicemaker = Unwrap<VoiceMaker>(args.This());
    /* text(string), [[speed(int32)], [modelFile(string)]] */
    if (args.Length() < 1 || !args[0]->IsString()) {
        return scope.Close(ThrowException(Exception::Error(String::New("Bad arguments. no text."))));
    }
    String::Utf8Value textString(args[0]->ToString());
    if (args.Length() >= 2) {
        if (args[1]->IsString()) {
            modelArgumentIndex = 1;
        } else if (args[1]->IsInt32()) {
            speed = args[1]->ToInt32()->Value();
            if (speed < 30 || speed > 300) {
                return scope.Close(ThrowException(Exception::Error(String::New("Bad arguments. speed is out of range."))));
            }
        } else {
            return scope.Close(ThrowException(Exception::Error(String::New("Bad arguments. second argument is invalid type."))));
        }
    }
    if (args.Length() == 3) {
        if (args[1]->IsString()) {
            return scope.Close(ThrowException(Exception::Error(String::New("Bad arguments. too many arguments."))));
        }
        if (!args[2]->IsString()) {
            return scope.Close(ThrowException(Exception::Error(String::New("Bad arguments. third argument is invalid type."))));
        }
        modelArgumentIndex = 2;
    }
    if (args.Length() >= 4) {
        return scope.Close(ThrowException(Exception::Error(String::New("Bad arguments. too many arguments."))));
    }
    if (modelArgumentIndex != -1) {
        String::Utf8Value modelFile(args[modelArgumentIndex]->ToString());
        return voicemaker->Convert(*textString, textString.length(), speed, *modelFile);
    } else {
        return voicemaker->Convert(*textString, textString.length(), speed, NULL);
    }
}

Handle<Value> VoiceMaker::GetErrorText(const Arguments& args) {
    HandleScope scope;
    char *errorText = "";

    if (args.Length() > 0) {
        return scope.Close(ThrowException(Exception::Error(String::New("Bad arguments. must be no argument."))));
    }
    VoiceMaker *voicemaker = Unwrap<VoiceMaker>(args.This());
    if (voicemaker->errorText) {
        errorText = voicemaker->errorText;
    }
    Local<String> errorString = String::New(errorText);
    return scope.Close(errorString);
}

Handle<Value> VoiceMaker::New(const Arguments& args) {
    HandleScope scope;
    VoiceMaker *voiceMaker = new VoiceMaker();
    voiceMaker->Wrap(args.This());
    return args.This();
 }

void VoiceMaker::Initialize(const Handle<Object>& target) {
    HandleScope scope;
    Local <FunctionTemplate> functionTemplate = FunctionTemplate::New(VoiceMaker::New);
    functionTemplate->InstanceTemplate()->SetInternalFieldCount(1);
    functionTemplate->SetClassName(String::NewSymbol("VoiceMaker"));
    NODE_SET_PROTOTYPE_METHOD(functionTemplate, "setDictionary", VoiceMaker::SetDictionary);
    NODE_SET_PROTOTYPE_METHOD(functionTemplate, "loadDictionary", VoiceMaker::LoadDictionary);
    NODE_SET_PROTOTYPE_METHOD(functionTemplate, "saveDictionary", VoiceMaker::SaveDictionary);
    NODE_SET_PROTOTYPE_METHOD(functionTemplate, "addPreferredWord", VoiceMaker::AddPreferredWord);
    NODE_SET_PROTOTYPE_METHOD(functionTemplate, "delPreferredWord", VoiceMaker::DelPreferredWord);
    NODE_SET_PROTOTYPE_METHOD(functionTemplate, "addFilterWord", VoiceMaker::AddFilterWord);
    NODE_SET_PROTOTYPE_METHOD(functionTemplate, "delFilterWord", VoiceMaker::DelFilterWord);
    NODE_SET_PROTOTYPE_METHOD(functionTemplate, "convert", VoiceMaker::Convert);
    NODE_SET_PROTOTYPE_METHOD(functionTemplate, "getErrorText", VoiceMaker::GetErrorText);
    target->Set(String::New("VoiceMaker"), functionTemplate->GetFunction());
}

extern "C" void init(Handle<Object> target) {
  VoiceMaker::Initialize(target);
}

} // namespace
