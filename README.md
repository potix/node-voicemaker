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
	voicemaker.loadDictionary();
	try { 
	     var waveData = this.data.voicemaker.convert("テキスト");
	     console.log(waveData);
	} catch (e) {
	     console.log(e.message);
	     console.log(this.data.voicemaker.getErrorText());
	}


辞書を設定する (入力値:ファイルパスを文字列で指定)

	voicemaker.setDictionary('./preferred.dic', './filter.dic');

辞書を読み込む

	voicemaker.loadDictionary();

辞書を保存する

	voicemaker.saveDictionary();

単語を登録する
	
	'voicemaker'という単語を'ボイスメーカー'という読みに変換
	voicemaker.addPreferredWord('voicemaker', 'ボイスメーカー');
	    
	'voicemaker'という文字列を'ボイスメーカー'という文字列に置換
	voicemaker.addFilterWord('voicemaker', 'ボイスメーカー');

単語を削除する

	voicemaker.delPreferredWord('voicemaker');
	voicemaker.delFilterWord('voicemaker');

テキストのみを指定して変換

	voicemaker.convert("喋らせたいテキスト");

スピードを指定する場合 (入力値:50-300)

	voicemaker.convert("喋らせたいテキスト", 80);

モデルファイルを指定する場合 (入力値:ファイルパスを文字列で指定)

	voicemaker.convert("喋らせたいテキスト", "/usr/local/share/aquestalk2/phont/aq_f1b.phont");

スピードとモデルファイルを指定する場合

	voicemaker.convert("喋らせたいテキスト", 80, "/usr/local/share/aquestalk2/phont/aq_f1b.phont");

変換処理でエラーが発生した場合のテキストを取得する

	voicemaker.getErrorText();


## About dictionary

preferred辞書はmecabの辞書変換時に使われ、mecabの辞書よりも優先される辞書です。

preferred辞書に登録した単語の読みに半角スペースが含まれていた場合正しく変換できません。

preferred辞書に'-'が登録されている場合、'-'の前が半角数字でない場合にのみ適応されます。

filter辞書はaqestalk2で音声変換を行う前に文字列置換を行うための辞書です。

filter辞書に半角スペースの登録をしても無視されます。

フィルタ辞書に半角'-'を登録しても無視されます。

preferred辞書、filter辞書共に半角数字の指定をしても無視されます。

同一辞書に同じキーを持つ単語を登録を登録した場合は先勝ちになります。


## Notes

mecabが--enable-sharedを付きでインストールされている必要があります。

utf-8エンコードのmecabの辞書がインストールされている必要があります。

aquestalk2がインストールされていなければなりません。

node.js-0.4.12でしか動作確認をしていません。


