# node-voicemaker

日本語テキストを音声データに変換するnode.jsのaddon。

なのでドキュメントも日本語で書いてます。

## Installing 

	TODO npm install


## Cloning the repository

	git clone git://github.com/potix/node-voicemaker.git

## Compiling and test

	make 

## Usage
        var preferredDic = __dirname + '/voicemaker_preferred.dic';
        var filterDic = __dirname + '/voicemaker_preferred.dic';
        var VoiceMaker = require('voicemaker').VoiceMaker;
        var voicemaker = new VoiceMaker();
        voicemaker.setDictionary(preferredDic, filterDic);

辞書を読み込む

        voicemaker.loadDictionary();

辞書を保存する

        voicemaker.saveDictionary();

単語を登録する

        voicemaker.addPreferredWord('voicemaker', 'ボイスメーカー');
        voicemaker.addFilterWord('voicemaker', 'ボイスメーカー');

単語を削除する

        voicemaker.delPreferredWord('voicemaker', 'ボイスメーカー');
        voicemaker.delFilterWord('voicemaker', 'ボイスメーカー');

テキストのみを指定して変換

	voicemaker.convert("喋らせたいテキスト");

スピードを指定する場合 (入力値:50-300)

	voicemaker.convert("喋らせたいテキスト", 80);

モデルファイルを指定する場合 (入力値:ファイルパスを文字列で指定)

	voicemaker.convert("喋らせたいテキスト", "/usr/local/share/aquestalk2/phont/aq_f1b.phont");

スピードとモデルファイルを指定する場合

	voicemaker.convert("喋らせたいテキスト", 80, "/usr/local/share/aquestalk2/phont/aq_f1b.phont");

## About dictionary

preferred辞書はmecabの辞書変換時に使われ、mecabの辞書よりも優先される辞書です。

filter辞書はaqestalk2で音声変換を行う前に文字列置換を行うための辞書です。

## Notes

mecabが--enable-sharedを付きでインストールされている必要があります。

utf-8エンコードのmecabの辞書がインストールされている必要があります。

aquestalk2がインストールされていなければなりません。

node.js-0.4.12でしか動作確認をしていません。



