let format = "audio-24khz-48kbitrate-mono-mp3";
// 采样率 ⚠ 格式后带有opus的实际采样率是其2倍
let sampleRate = 24000; // 对应24khz

let PluginJS = {
    "name": "微软翻译",
    "id": "com.microsoft.translator", // 插件的唯一ID 建议以此种方式命名
    "author": "TTS Server",

    "description": "使用的微软翻译Android客户端的Azure TTS接口",
    "version": 4, // 插件版本号

    // 获取音频
    // text: 文本，locale：地区代码，voice：声音key，rate：语速，volume：音量，pitch：音高。(后三者范围都在100内, 当随系统时在200内)
    "getAudio": function (text, locale, voice, rate, volume, pitch) {
        rate = (rate * 2) - 100;
        pitch = pitch - 50;

        let styleDegree = ttsrv.tts.data['styleDegree'];
        if (!styleDegree || Number(styleDegree) < 0.01) {
            styleDegree = '1.0';
        }

        let style = ttsrv.tts.data['style'];
        let role = ttsrv.tts.data['role'];
        if (!style || style === "") {
            style = 'general';
        }
        if (!role || role === "") {
            role = 'default';
        }

        let textSsml = '';
        let langSkill = ttsrv.tts.data['languageSkill'];

        if (langSkill === "" || langSkill == null) {
            textSsml = specialCharReplace(text);
        } else {
            textSsml = `<lang xml:lang="${langSkill}">${specialCharReplace(text)}</lang>`;
        }

        let ssml = `
        <speak xmlns="http://www.w3.org/2001/10/synthesis" xmlns:mstts="http://www.w3.org/2001/mstts" xmlns:emo="http://www.w3.org/2009/10/emotionml" version="1.0" xml:lang="zh-CN">
            <voice name="${voice}">
                <mstts:express-as style="${style}" styledegree="${styleDegree}" role="${role}">
                    <prosody rate="${rate}%" pitch="${pitch}%" volume="${volume}">${textSsml}</prosody>
                </mstts:express-as>
            </voice>
        </speak>
        `;

        return getAudioInternal(ssml, format);
    },
};

function specialCharReplace(s) {
    return s.replace("'", "&apos;").replace('"', '&quot;').replace('<', '&lt;').replace('>', '&gt;').replace('&', '&amp;').replace('/', '').replace('\\', '');
}

let ep = "";

function getAudioInternal(ssml, format) {
    if (ep === "") {
        ep = getEndpoint();
    }

    let url = "https://" + ep['r'] + ".tts.speech.microsoft.com/cognitiveservices/v1";
    let headers = {
        'Authorization': ep['t'],
        "Content-Type": "application/ssml+xml",
        "X-Microsoft-OutputFormat": format,
    };
    let resp = ttsrv.httpPost(url, ssml, headers);
    if (resp.code() !== 200) {
        ep = '';
        throw "音频获取失败: HTTP-" + resp.code();
    }

    return resp.body().byteStream();
}

let endpointUrl = "https://dev.microsofttranslator.com/apps/endpoint?api-version=1.0";

function getEndpoint() {
    let sign = getSign();

    let headers = {
        "Accept-Language": "zh-Hans",
        "X-ClientVersion": "4.0.530a 5fe1dc6c",
        "X-UserId": "0f04d16a175c411e",
        "X-HomeGeographicRegion": "zh-Hans-CN",
        "X-ClientTraceId": "aab069b9-70a7-4844-a734-96cd78d94be9",
        "X-MT-Signature": sign,
        "User-Agent": "okhttp/4.5.0",
        "Content-Type": "application/json; charset=utf-8",
        "Content-Length": "0",
        "Accept-Encoding": "gzip"
    };

    let resp = ttsrv.httpPost(endpointUrl, "", headers);
    if (resp.code() !== 200) {
        throw "终结点信息获取失败: HTTP-" + resp.code();
    }
    let str = resp.body().string();
    return JSON.parse(str);
}

function getSign() {
    let aly = new JavaImporter(
        javax.crypto.Mac,
        javax.crypto.spec.SecretKeySpec,
        java.net.URLEncoder,
        java.lang.String,
        java.text.SimpleDateFormat,
        java.util.Locale,
        java.util.TimeZone,
        android.util.Base64
    );
    with (aly) {
        function percentEncode(value) {
            return URLEncoder.encode(value, 'UTF-8');
        }

        function dateFormat() {
            let simpleDateFormat = new SimpleDateFormat("EEE, dd MMM yyyy HH:mm:ss", Locale.US);
            simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));
            return simpleDateFormat.format(new Date()).toLowerCase() + "GMT";
        }

        function sign(url) {
            url = url.split('://')[1];

            let encodeUrl = percentEncode(url);
            let uuid = ttsrv.randomUUID().replaceAll("-", "");
            let formattedDate = dateFormat();
            let bytes = String.format("%s%s%s%s", "MSTranslatorAndroidApp", encodeUrl, formattedDate, uuid).toLowerCase().getBytes('UTF-8');

            let secretKeySpec = new SecretKeySpec(Base64.decode("oik6PdDdMnOXemTbwvMn9de/h9lFnfBaCWbGMMZqqoSaQaqUOqjVGm5NqsmjcBI1x+sS9ugjB55HEJWRiFXYFw==", 2), "HmacSHA256");
            var mac = Mac.getInstance('HmacSHA256');
            mac.init(secretKeySpec);
            var signData = mac.doFinal(bytes);
            var signBase64 = Base64.encodeToString(signData, Base64.NO_WRAP);
            return String.format("%s::%s::%s::%s", "MSTranslatorAndroidApp", signBase64, formattedDate, uuid);
        }
    }

    return sign(endpointUrl);
}

// 全部voice数据
let voices = {};
// 当前语言下的voice
let currentVoices = new Map();

// 语言技能 二级语言
let skillSpinner;

let styleSpinner;
let roleSpinner;
let seekStyle;

let EditorJS = {
    // 音频的采样率 编辑TTS界面保存时调用
    "getAudioSampleRate": function (locale, voice) {
        // 根据voice判断返回的采样率
        // 也可以动态获取：
        return sampleRate;
    },

    "getLocales": function () {
        let locales = new Array();

        voices.forEach(function (v) {
            let loc = v["Locale"];
            if (!locales.includes(loc)) {
                locales.push(loc);
            }
        });

        return locales;
    },

    // 当语言变更时调用
    "getVoices": function (locale) {
        currentVoices = new Map();
        voices.forEach(function (v) {
            if (v['Locale'] === locale) {
                currentVoices.set(v['ShortName'], v);
            }
        });

        let mm = {};
        for (let [key, value] of currentVoices.entries()) {
            mm[key] = new java.lang.String(value['LocalName'] + ' (' + key + ')');
        }
        return mm;
    },

    // 加载本地或网络数据，运行在IO线程。
    "onLoadData": function () {
        // 获取数据并缓存以便复用
        let jsonStr = '';
        if (ttsrv.fileExist('voices.json')) {
            jsonStr = ttsrv.readTxtFile('voices.json');
        } else {
            let ep = getEndpoint();
            let url = 'https://' + ep['r'] + '.tts.speech.microsoft.com/cognitiveservices/voices/list';
            let header = {
                "Content-Type": "application/json",
                "Authorization": ep['t'],
            };
            jsonStr = ttsrv.httpGetString(url, header);
            ttsrv.writeTxtFile('voices.json', jsonStr);
        }

        voices = JSON.parse(jsonStr);
    },

    "onLoadUI": function (ctx, linerLayout) {
        let layout = new LinearLayout(ctx);
        layout.orientation = LinearLayout.HORIZONTAL; // 水平布局
        let params = new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1);

        skillSpinner = JSpinner(ctx, "语言技能 (language skill)");
        linerLayout.addView(skillSpinner);
        ttsrv.setMargins(skillSpinner, 2, 4, 0, 0);
        skillSpinner.setOnItemSelected(function (spinner, pos, item) {
            ttsrv.tts.data['languageSkill'] = item.value + '';
        });

        styleSpinner = JSpinner(ctx, "风格 (style)");
        styleSpinner.layoutParams = params;
        layout.addView(styleSpinner);
        ttsrv.setMargins(styleSpinner, 2, 4, 0, 0);
        styleSpinner.setOnItemSelected(function (spinner, pos, item) {
            ttsrv.tts.data['style'] = item.value;
            // 默认 || value为空 || value空字符串
            if (pos === 0 || !item.value || item.value === "") {
                seekStyle.visibility = View.GONE; // 移除风格强度
            } else {
                seekStyle.visibility = View.VISIBLE; // 显示
            }
        });

        roleSpinner = JSpinner(ctx, "角色 (role)");
        roleSpinner.layoutParams = params;
        layout.addView(roleSpinner);
        ttsrv.setMargins(roleSpinner, 0, 4, 2, 0);
        roleSpinner.setOnItemSelected(function (spinner, pos, item) {
            ttsrv.tts.data['role'] = item.value;
        });
        linerLayout.addView(layout);

        seekStyle = JSeekBar(ctx, "风格强度 (Style degree)：");
        linerLayout.addView(seekStyle);
        ttsrv.setMargins(seekStyle, 0, 4, 0, -4);
        seekStyle.setFloatType(2); // 二位小数
        seekStyle.max = 200; // 最大200个刻度

        let styleDegree = Number(ttsrv.tts.data['styleDegree']);
        if (!styleDegree || isNaN(styleDegree)) {
            styleDegree = 1.0;
        }
        seekStyle.value = new java.lang.Float(styleDegree);

        seekStyle.setOnChangeListener({
            // 开始时
            onStartTrackingTouch: function (seek) {

            },
            // 进度滑动更改时
            onProgressChanged: function (seek, progress, fromUser) {

            },
            // 停止时
            onStopTrackingTouch: function (seek) {
                ttsrv.tts.data['styleDegree'] = Number(seek.value).toFixed(2);
            },
        });
    },

    "onVoiceChanged": function (locale, voiceCode) {
        let vic = currentVoices.get(voiceCode);

        let locale2List = vic['SecondaryLocaleList'];
        let locale2Items = [];
        let locale2Pos = 0;

        if (locale2List) {
            locale2Items.push(Item("默认 (default)", ""));
            locale2List.map(function (v, i) {
                let loc = java.util.Locale.forLanguageTag(v);
                let name = loc.getDisplayName(loc);
                locale2Items.push(Item(name, v));
                if (v === ttsrv.tts.data['languageSkill'] + '') {
                    locale2Pos = i + 1;
                }
            });
        }
        skillSpinner.items = locale2Items;
        skillSpinner.selectedPosition = locale2Pos;

        if (locale2Items.length === 0) {
            skillSpinner.visibility = View.GONE;
        } else {
            skillSpinner.visibility = View.VISIBLE;
        }

        let styles = vic['StyleList'];
        let styleItems = [];
        let stylePos = 0;
        if (styles) {
            styleItems.push(Item("默认 (general)", ""));
            styles.map(function (v, i) {
                styleItems.push(Item(getString(v), v));
                if (v === ttsrv.tts.data['style'] + '') {
                    stylePos = i + 1; // 算上默认的item 所以要 +1
                }
            });
        } else {
            seekStyle.visibility = View.GONE;
        }
        styleSpinner.items = styleItems;
        styleSpinner.selectedPosition = stylePos;

        let roles = vic['RolePlayList'];
        let roleItems = [];
        let rolePos = 0;
        if (roles) {
            roleItems.push(Item("默认 (default)", ""));
            roles.map(function (v, i) {
                roleItems.push(Item(getString(v), v));
                if (v === ttsrv.tts.data['role'] + '') {
                    rolePos = i + 1; // 算上默认的item 所以要 +1
                }
            });
        }
        roleSpinner.items = roleItems;
        roleSpinner.selectedPosition = rolePos;
    }
};

let cnLocales = {
    "girl": "女孩",
    "boy": "男孩",
    "youngadultfemale": "青年女",
    "youngadultmale": "青年男",
    "olderadultfemale": "中年女",
    "olderadultmale": "中年男",
    "seniorfemale": "老年女",
    "seniormale": "老年男",

    "advertisement_upbeat": "广告",
    "affectionate": "亲切",
    "angry": "愤怒",
    "assistant": "助理",
    "calm": "平静",
    "chat": "聊天",
    "cheerful": "快乐",
    "customerservice": "客服",
    "depressed": "沮丧",
    "disgruntled": "不满",
    "documentary-narration": "纪录片",
    "embarrassed": "尴尬",
    "empathetic": "同情",
    "envious": "嫉妒",
    "excited": "兴奋",
    "fearful": "害怕",
    "friendly": "友好",
    "gentle": "温柔",
    "hopeful": "希望",
    "lyrical": "抒情",
    "narration-professional": "专业",
    "narration-relaxed": "轻松",
    "newscast": "新闻",
    "newscast-casual": "新闻-休闲",
    "newscast-formal": "新闻-正式",
    "poetry-reading": "诗歌",
    "sad": "伤心",
    "serious": "严肃",
    "shouting": "大声",
    "sports_commentary": "体育",
    "sports_commentary_excited": "体育-兴奋",
    "whispering": "低语",
    "terrified": "恐惧",
    "unfriendly": "不友好",
};

let isZh = java.util.Locale.getDefault().getLanguage() == 'zh';
function getString(key) {
    if (isZh) {
        return cnLocales[key.toLowerCase()] || key;
    } else {
        return key;
    }
}

