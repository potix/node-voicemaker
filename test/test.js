var VoiceMaker = require('../build/default/voicemaker').VoiceMaker;
var voicemaker = new VoiceMaker();
console.log(voicemaker.convert("私は、モモンガの次男の孫の長男の従兄弟のへべれけという者です。"));
console.log(voicemaker.convert("私は、モモンガの次男の孫の長男の従兄弟のへべれけという者です。", "/usr/local/share/aquestalk2/phont/aq_m3.phont"));
console.log(voicemaker.convert("私は、モモンガの次男の孫の長男の従兄弟のへべれけという者です。", 80, "/usr/local/share/aquestalk2/phont/aq_m3.phont"));
