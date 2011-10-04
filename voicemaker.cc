#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <v8.h>
#include <node.h>
#include <AquesTalk2.h>
#include <mecab.h>

using namespace v8;
using namespace node;
using namespace MeCab;
using namespace std;

namespace {

class VoiceMaker: ObjectWrap {
public:
    static void Initialize(const Handle<Object>& target);
    static Handle<Value> New(const Arguments& args);
    static Handle<Value> Convert(const Arguments& args);

    VoiceMaker();
    ~VoiceMaker();

private:
    const char *base64char;
 
    void ConvertFree(char *newText, mecab_t *mecab, char *filterdText, unsigned char *modelData, unsigned char *waveData);
    Handle<Value> Convert(const char* text, int textLength, int speed, const char *modelFile);

    void FilterFree(char *newText);
    int Filter(char **filterdText, char *text);

    void LoadFileFree(unsigned char *fileData);
    int LoadFile(const char *filePath, unsigned char **fileData, size_t *fileSize);

    void Base64EncodeFree(char *out);
    int Base64Encode(char **out, int *outLen, const unsigned char *in, int inSize);
};

VoiceMaker::VoiceMaker() {
    base64char = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}

VoiceMaker::~VoiceMaker() {
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

void VoiceMaker::FilterFree(char *filterdText) {
    free(filterdText);
}

struct filterMap {
    const char *src;
    int srcLen;
    const char *dst;
    int dstLen;
};
typedef struct filterMap filterMap_t;

int VoiceMaker::Filter(char **filterdText, char *text) {
    int i;
    char *newText, *newTextBack;
    int textLength, newTextLength;
    char *exist;
    filterMap_t *filter;
    filterMap_t filterTable[]={
        { "！", sizeof("！") -1, "", sizeof("") -1 },
        { "!", sizeof("!") -1, "", sizeof("") -1 },
        { "＝", sizeof("＝") -1, "イコール", sizeof("イコール") -1 },
        { "=", sizeof("=") -1, "イコール", sizeof("イコール") -1 },
        { "　", sizeof("　") -1, "", sizeof("") -1 },
        { " ", sizeof(" ") -1, "", sizeof("") -1 },
        { "づ", sizeof("づ") -1, "ず", sizeof("ず") -1 },
        { "ヅ", sizeof("ヅ") -1, "ズ", sizeof("ズ") -1 },
        { "ゔぁ", sizeof("ゔぁ") -1, "ば", sizeof("ば") -1 },
        { "ヴァ", sizeof("ヴァ") -1, "バ", sizeof("バ") -1 },
        { "ゔぃ", sizeof("ゔぃ") -1, "び", sizeof("び") -1 },
        { "ヴィ", sizeof("ヴィ") -1, "ビ", sizeof("ビ") -1 },
        { "ゔ", sizeof("ゔ") -1, "ぶ", sizeof("ぶ") -1 },
        { "ヴ", sizeof("ヴ") -1, "ブ", sizeof("ブ") -1 },
        { "ゔぇ", sizeof("ゔぇ") -1, "べ", sizeof("べ") -1 },
        { "ヴェ", sizeof("ヴェ") -1, "ベ", sizeof("ベ") -1 },
        { "ゔぉ", sizeof("ゔぉ") -1, "ぼ", sizeof("ぼ") -1 },
        { "ヴォ", sizeof("ヴォ") -1, "ボ", sizeof("ボ") -1 },
        { "ぁ", sizeof("ぁ") -1, "あ", sizeof("あ") -1 },
        { "ぃ", sizeof("ぃ") -1, "い", sizeof("い") -1 },
        { "ぅ", sizeof("ぅ") -1, "う", sizeof("う") -1 },
        { "ぇ", sizeof("ぇ") -1, "え", sizeof("え") -1 },
        { "ぉ", sizeof("ぉ") -1, "お", sizeof("お") -1 },
        { "き", sizeof("き") -1, "キ", sizeof("キ") -1 },
        { "し", sizeof("し") -1, "シ", sizeof("シ") -1 },
        { "ひ", sizeof("ひ") -1, "ヒ", sizeof("ヒ") -1 },
        { "に", sizeof("に") -1, "ニ", sizeof("ニ") -1 },
        { "み", sizeof("み") -1, "ミ", sizeof("ミ") -1 },
        { "り", sizeof("り") -1, "リ", sizeof("リ") -1 },
        { "ぎ", sizeof("ぎ") -1, "ギ", sizeof("ギ") -1 },
        { "じ", sizeof("じ") -1, "ジ", sizeof("ジ") -1 },
        { "び", sizeof("び") -1, "ビ", sizeof("ビ") -1 },
        { "ゃ", sizeof("ゃ") -1, "ャ", sizeof("ャ") -1 },
        { "ゅ", sizeof("ゅ") -1, "ュ", sizeof("ュ") -1 },
        { "ょ", sizeof("ょ") -1, "ョ", sizeof("ョ") -1 },
    };

    if (filterdText == NULL ||
        text == NULL ||
        textLength < 1) {
        return 1;
    }
    textLength = strlen(text);
    newTextLength = textLength * 8;
    newText = (char *)malloc(newTextLength);
    if (!newText) {
        return 2;
    }
    newTextBack = (char *)malloc(newTextLength);
    if (!newTextBack) {
        free(newText);
        return 3;
    }
    strcpy(newText, text);
    for (i = 0; i < sizeof(filterTable)/sizeof(filterTable[0]); i++ ) {
       filter = &filterTable[i];
       if (textLength < filter->srcLen) {
           continue;
       }
       if (newTextLength < filter->dstLen) {
           continue;
       }
       while(1) {
           exist = strstr(newText, filter->src);
           if (!exist) {
               break;
           }
           strcpy(newTextBack, exist);
           strcpy(exist, filter->dst);
           strcpy(exist + filter->dstLen, newTextBack + filter->srcLen);
       }
    }
    free(newTextBack);
    *filterdText = newText;

    return 0;
}

void VoiceMaker::ConvertFree(char *newText, mecab_t *mecab, char *filterdText, unsigned char *modelData, unsigned char *waveData) {
    free(newText);
    if (mecab) {
        mecab_destroy(mecab);
    }
    if (filterdText) {
        FilterFree(filterdText);
    }
    if (modelData) {
        LoadFileFree(modelData);
    }
    if (waveData) {
        AquesTalk2_FreeWave(waveData);
    }
}

Handle<Value> VoiceMaker::Convert(const char* text, int textLength, int speed,  const char *modelFile) {
    HandleScope scope;
    const char *error = NULL;
    mecab_t *mecab = NULL;
    const mecab_node_t *node;
    int argc = 1;
    char *argv[] = { "voicemaker" };
    char *newText = NULL;
    char *newTextPtr = NULL;
    char *filterdText = NULL;
    int newTextLength = textLength * 15 * 4;
    unsigned char *modelData = NULL;
    size_t modelSize;
    unsigned char *waveData = NULL;
    int waveSize;
    char *waveBase64 = NULL;
    int waveBase64Len;
    int result;

    newText = (char *)malloc(newTextLength);
    if (!newText) {
         error = "failed in allocate buffer of new text.";
         return ThrowException(Exception::Error(String::New(error)));
    }
    newTextPtr = newText;

    mecab = mecab_new(argc, argv);
    if (!mecab) {
         ConvertFree(newText, mecab, filterdText, modelData, waveData);
         error = "failed in create instance of Mecab::Tagger.";
         return ThrowException(Exception::Error(String::New(error)));
    }
    node = mecab_sparse_tonode(mecab, text);
    if (!node) {
         ConvertFree(newText, mecab, filterdText, modelData, waveData);
         error = "failed in create instance of Mecab::Node.";
         return ThrowException(Exception::Error(String::New(error)));
    }
    for (; node; node = node->next) {
         const char *startPtr = NULL;
         const char *endPtr = NULL;
         const char *currentPtr = node->feature;
         int delimiter = 0;
         int length;
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
         } else {
             if (startPtr && endPtr) {
                 length = endPtr - startPtr;
                 memcpy(newTextPtr, startPtr, length);
                 newTextPtr += length;
             }
         }
    }
    mecab_destroy(mecab);
    mecab = NULL;
    *newTextPtr = '\0';
    if ((result = Filter(&filterdText, newText))) {
        ConvertFree(newText, mecab, filterdText, modelData, waveData);
        switch (result) {
        case 1:
            error = "failed in filtering. invalid argument.";
            break;
        case 2:
            error = "failed in filtering. failed in allocate memory of filterd text.";
            break;
        case 3:
            error = "failed in filtering. failed in allocate memory of backup filterd text.";
            break;
        default:
            error = "failed in load file of model. unkown.";
            break;
        }
        return ThrowException(Exception::Error(String::New(error)));
    }
    free(newText);
    newText = NULL;

    if (modelFile) {
        if ((result = LoadFile(modelFile, &modelData, &modelSize))) {
            ConvertFree(newText, mecab, filterdText, modelData, waveData);
            switch (result) {
            case 1:
                error = "failed in load file of model. not found model file.";
                break;
            case 2:
                error = "failed in load file of model. failed in allocate memory of model data.";
                break;
            case 3:
                error = "failed in load file of model. failed in open file of model.";
                break;
            case 4:
                error = "failed in load file of model. failed in read data of model.";
                break;
            default:
                error = "failed in load file of model. unkown.";
                break;
            }
            return ThrowException(Exception::Error(String::New(error)));
        }
    }
    waveData = AquesTalk2_Synthe_Utf8(filterdText, speed, &waveSize, modelData);
    if (!waveData) {
        ConvertFree(newText, mecab, filterdText, modelData, waveData);
        error = "failed in create data of wave.";
        return ThrowException(Exception::Error(String::New(error)));
    }
    free(filterdText);
    filterdText = NULL;
    LoadFileFree(modelData);
    modelData = NULL;
    if (Base64Encode(&waveBase64, &waveBase64Len, waveData, waveSize)) {
        ConvertFree(newText, mecab, filterdText, modelData, waveData);
        error = "failed in encode to base64.";
        return ThrowException(Exception::Error(String::New(error)));
    }
    AquesTalk2_FreeWave(waveData);
    Local<String> dataString = String::New(waveBase64);
    Base64EncodeFree(waveBase64);

    return scope.Close(dataString);
}

Handle<Value> VoiceMaker::Convert(const Arguments& args) {
    HandleScope scope;
    int modelArgumentIndex = -1;
    int speed = 100;

    VoiceMaker *voicemaker = Unwrap<VoiceMaker>(args.This());
    /* text(string), [[speed(int32)], [modelFile(string)]] */
    if (args.Length() < 1 || !args[0]->IsString()) {
        return ThrowException(Exception::Error(String::New("Bad arguments. no text.")));
    }
    String::Utf8Value textString(args[0]->ToString());
    if (args.Length() >= 2) {
        if (args[1]->IsString()) {
            modelArgumentIndex = 1;
        } else if (args[1]->IsInt32()) {
            speed = args[1]->ToInt32()->Value();
            if (speed < 30 || speed > 300) {
                return ThrowException(Exception::Error(String::New("Bad arguments. speed is out of range.")));
            }
        } else {
            return ThrowException(Exception::Error(String::New("Bad arguments. second argument is invalid type.")));
        }
    }
    if (args.Length() == 3) {
        if (args[1]->IsString()) {
            return ThrowException(Exception::Error(String::New("Bad arguments. too many arguments.")));
        }
        if (!args[2]->IsString()) {
            return ThrowException(Exception::Error(String::New("Bad arguments. third argument is invalid type.")));
        }
        modelArgumentIndex = 2;
    }
    if (args.Length() >= 4) {
        return ThrowException(Exception::Error(String::New("Bad arguments. too many arguments.")));
    }
    if (modelArgumentIndex != -1) {
        String::Utf8Value modelFile(args[modelArgumentIndex]->ToString());
        return voicemaker->Convert(*textString, textString.length(), speed, *modelFile);
    } else {
        return voicemaker->Convert(*textString, textString.length(), speed, NULL);
    }
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
    NODE_SET_PROTOTYPE_METHOD(functionTemplate, "convert", VoiceMaker::Convert);
    target->Set(String::New("VoiceMaker"), functionTemplate->GetFunction());
}

extern "C" void init(Handle<Object> target) {
  VoiceMaker::Initialize(target);
}

} // namespace
