# node-voicemaker

日本語テキストを音声データに変換するnode.jsのaddon
なのでドキュメントも日本語で書いてます

## Installing 

	TODO npm install


## Cloning the repository

	git clone git://github.com/potix/node-voicemaker.git

## Compiling and test

	make 

## Usage

	var VoiceMaker = require('voicemaker').VoiceMaker;
	var voicemaker = new VoiceMaker();
	voicemaker.convert("喋らせたいテキスト");

テキストのみの場合
	voicemaker.convert("喋らせたいテキスト");
スピードを指定する場合 (入力値:50-300)
	voicemaker.convert("喋らせたいテキスト", 80);
モデルファイルを指定する場合 (入力値:ファイルパスを文字列で指定)
	voicemaker.convert("喋らせたいテキスト", "/usr/local/share/aquestalk2/phont/aq_f1b.phont");
スピードとモデルファイルを指定する場合
	voicemaker.convert("喋らせたいテキスト", 80, "/usr/local/share/aquestalk2/phont/aq_f1b.phont");

## Notes

mecabが--enable-sharedを付きでインストールされている必要があります。
utf-8エンコードのmecabの辞書がインストールされている必要があります。
aquestalk2がインストールされていなければなりません。
node.js-0.4.12でしか動作確認をしていません。

