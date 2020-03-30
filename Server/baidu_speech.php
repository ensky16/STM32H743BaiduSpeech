<?php
header("Content-Type: text/html;charset=utf-8");
define('DEMO_CURL_VERBOSE', false); // ´òÓ¡curl debugÐÅÏ¢
 
//global variables
# 填写网页上申请的appkey 如 $apiKey="g8eBUMSokVB1BHGmgxxxxxx"
$API_KEY =  "kVcnfD9iW2XVZSMaLMrtLYIz";
# 填写网页上申请的APP SECRET 如 $secretKey="94dc99566550d87f8fa8ece112xxxxx"
$SECRET_KEY = "O9o1O213UgG5LFn0bDGNtoRN3VWl2du6";
$qingyunke_url = 'http://api.qingyunke.com/api.php?key=free&appid=0&msg=';
$audio_file_name="./receive16k.pcm";

$token="";
$g_has_error = true;
$ulr="";
$paramsStr="";
$baidu_tts_result_file_name="";

baidu_get_token($API_KEY, $SECRET_KEY);

$baiduAsrResult_utf8=baidu_asr($audio_file_name, $token);

$stringData=urlencode($baiduAsrResult_utf8);
$finalGetUrl=$qingyunke_url.$stringData;
$chatResult_utf8=file_get_contents($finalGetUrl); 
$chatResUtf8DecodeJson=json_decode( $chatResult_utf8);
$robotReply_utf8= $chatResUtf8DecodeJson->content;

$robotReply_gbk= iconv("UTF-8","gbk//TRANSLIT",$robotReply_utf8);
$baiduAsrResult_gbk=iconv("UTF-8","gbk//TRANSLIT",$baiduAsrResult_utf8);


baidu_tts_init($robotReply_utf8, $token);
// echo "<br />";
// echo "<br />";

baidu_tts_request($url, $paramsStr);

//debug
// echo "<br />";
// echo "<br />";
// echo "ffmpeg: ";
// echo shell_exec("ffmpeg -y -i $file -f wav -ar 44100 -ac 2 result.wav");
shell_exec(" ffmpeg -y -i $baidu_tts_result_file_name -f wav -ar 44100 -ac 2 result.wav");
// echo "<br />";
// echo "<br />";

//debug
// $baidu_tts_result_file_name="test.txt";
$file_path="result.wav";
// echo "<br />";
// echo "<br />";
// echo  $baidu_tts_result_file_name;
// echo "<br />";
// echo "<br />";
// $file_path="result.wav";
if(file_exists($file_path))
{
	$baidu_tts_file_content = file_get_contents($file_path);//将整个文件内容读入到一个字符串中
}

// echo "<br />";
// echo "999 file context is ";
// echo "<br />";

// echo  $baidu_tts_file_content;
// file_put_contents("testTTS.mp3", $baidu_tts_file_content);   

// echo "<br />";
// echo "999 file context base64_encode(data) ";
// echo "<br />";
$robotReply_audio=base64_encode($baidu_tts_file_content);
// $robotReply_audio=$baidu_tts_file_content;
// echo  $robotReply_audio;
// echo "<br />";
// file_put_contents("testTTS_fordecode.mp3", base64_decode($robotReply_audio));   

echo "{";
echo '"'."asrResultUtf8\":"."\"".$baiduAsrResult_utf8.'"'.","; 
echo '"'."asrResultGBK\":"."\"".$baiduAsrResult_gbk.'"'.","; 
echo '"'."robosResultUtf8\":"."\"".$robotReply_utf8.'"'.","; 
echo '"'."robosResultGBK\":"."\"".$robotReply_gbk.'"'.","; 
echo '"'."robosResultAudio\":"."\"".$robotReply_audio."\""; 
echo "}";


// echo "\r\n this is the end of post, 9988\r\n";
 
// file_put_contents("baidu_speech_result.txt", $res);   


// echo "<br />";
// echo "999 file context is ";
// echo "<br />";
// echo $robotReply_audio;
// for($i=0; $i<2000; $i++)
// {
//    printf("%x ",$baidu_tts_file_content[$i]);

// }

function baidu_asr($audio_file_name, $token)
{
	$CUID = "123456PHP";
	# 采样率
	$RATE = 16000;  // 固定值

	# 普通版
	$ASR_URL = "http://vop.baidu.com/server_api";
	# 根据文档填写PID，选择语言及识别模型
	$DEV_PID = 1537; //  1537 表示识别普通话，使用输入法模型。
	//$SCOPE = 'audio_voice_assistant_get'; // 有此scope表示有语音识别普通版能力，没有请在网页里开通语音识别能力

	$AUDIO_FILE = $audio_file_name;
	
	// //debug
	// echo "</br>";
	// echo $AUDIO_FILE;
	// echo "</br>";

	$FORMAT = substr($AUDIO_FILE, -3); // 文件后缀 pcm/wav/amr 格式 极速版额外支持m4a 格式

	/** 拼接参数开始 **/
	$audio = file_get_contents($AUDIO_FILE);
		
	$url = $ASR_URL . "?cuid=".$CUID. "&token=" . $token . "&dev_pid=" . $DEV_PID;
	/*测试自训练平台需要开启下面的注释*/	
	//$url = $ASR_URL . "?cuid=".$CUID. "&token=" . $token . "&dev_pid=" . $DEV_PID . "&lm_id=" . $LM_ID;
	$headers[] = "Content-Length: ".strlen($audio);
	$headers[] = "Content-Type: audio/$FORMAT; rate=$RATE";
	/** 拼接参数结束 **/
		
	$ch = curl_init();
	curl_setopt($ch, CURLOPT_URL, $url);
	curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
	curl_setopt($ch, CURLOPT_HTTPHEADER, $headers);
	curl_setopt($ch, CURLOPT_POST, 1);
	curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, 5);
	curl_setopt($ch, CURLOPT_TIMEOUT, 60); // Ê¶±ðÊ±³¤²»³¬¹ýÔ­Ê¼ÒôÆµ
	curl_setopt($ch, CURLOPT_POSTFIELDS, $audio);
	curl_setopt($ch, CURLOPT_VERBOSE, DEMO_CURL_VERBOSE);
	$res = curl_exec($ch);
	if(curl_errno($ch))
	{
	    print curl_error($ch);
	}
	curl_close($ch); 

	$response = json_decode($res, true);

	if (isset($response['err_no']) && $response['err_no'] == 0)
	{
		$baidu_asr_result= $response['result'][0];
	}else{
		$baidu_asr_result="baidu asr error";
	}

	return $baidu_asr_result;
}



function baidu_get_token($API_KEY, $SECRET_KEY, $SCOPE )
{
	global $token;
	$SCOPE = false; // 部分历史应用没有加入scope，设为false忽略检查
	/** 公共模块获取token开始 */
	$auth_url = "http://openapi.baidu.com/oauth/2.0/token?grant_type=client_credentials&client_id=".$API_KEY."&client_secret=".$SECRET_KEY;
	$ch = curl_init();
	curl_setopt($ch, CURLOPT_URL, $auth_url);
	curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
	curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, 5);
	curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false); //信任任何证书
	curl_setopt($ch, CURLOPT_SSL_VERIFYHOST, 0); // 检查证书中是否设置域名,0不验证
	curl_setopt($ch, CURLOPT_VERBOSE, DEMO_CURL_VERBOSE);
	$res = curl_exec($ch);
	if(curl_errno($ch))
	{
	    print curl_error($ch);
	}
	curl_close($ch);

	//echo "Token URL response is " . $res . "\n";
	$response = json_decode($res, true);

	if (!isset($response['access_token'])){
		echo "ERROR TO OBTAIN TOKEN\n";
		exit(1);
	}
	if (!isset($response['scope'])){
		echo "ERROR TO OBTAIN scopes\n";
		exit(2);
	}

	if ($SCOPE && !in_array($SCOPE, explode(" ", $response['scope']))){
		echo "CHECK SCOPE ERROR\n";
		// 请至网页上应用内开通语音识别权限
		exit(3);
	}

	$token = $response['access_token'];
	//echo "token = $token ; expireInSeconds: ${response['expires_in']}\n\n";
	/** 公共模块获取token结束 */
}

$format;
function baidu_tts_init($text, $token)
{
	global $url;
	global $paramsStr;
	global $format;
	# 发音人选择, 基础音库：0为度小美，1为度小宇，3为度逍遥，4为度丫丫，
	# 精品音库：5为度小娇，103为度米朵，106为度博文，110为度小童，111为度小萌，默认为度小美 
	$per = 0;
	#语速，取值0-15，默认为5中语速
	$spd = 5;
	#音调，取值0-15，默认为5中语调
	$pit = 5;
	#音量，取值0-9，默认为5中音量
	$vol = 5;
	// 下载的文件格式, 3：mp3(default) 4： pcm-16k 5： pcm-8k 6. wav
	$aue = 3;

	$formats = array(3 => 'mp3', 4 => 'pcm', 5 =>'pcm', 6 => 'wav');
	$format = $formats[$aue];

	$cuid = "123456PHP";

	/** 拼接参数开始 **/
	// tex=$text&lan=zh&ctp=1&cuid=$cuid&tok=$token&per=$per&spd=$spd&pit=$pit&vol=$vol
	$params = array(
		'tex' => urlencode($text), // 为避免+等特殊字符没有编码，此处需要2次urlencode。
		'per' => $per,
		'spd' => $spd,
		'pit' => $pit,
		'vol' => $vol,
		'aue' => $aue,
		'cuid' => $cuid,
		'tok' => $token,
		'lan' => 'zh', //固定参数
		'ctp' => 1, // 固定参数
	);
	$paramsStr =  http_build_query($params);
	$url = 'http://tsn.baidu.com/text2audio';
	$urltest = $url . '?' . $paramsStr;
	// echo $urltest . "\n"; // 反馈请带上此url
	/** 拼接参数结束 **/	
}

function baidu_tts_request($url, $paramsStr)
{
	global $g_has_error;
	global $format;
	global $baidu_tts_result_file_name;
	$g_has_error = true;
 	$ch = curl_init();
	curl_setopt($ch, CURLOPT_URL, $url);
	curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
	curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, 5);
	curl_setopt ($ch, CURLOPT_POST, 1);
	curl_setopt($ch, CURLOPT_POSTFIELDS, $paramsStr);	
	curl_setopt($ch, CURLOPT_HEADERFUNCTION, 'read_header');
	// $header="Content-Type: text/html;charset=utf-8";
	// curl_setopt($ch, CURLOPT_HEADERFUNCTION, read_header($ch, $header));
	$data = curl_exec($ch);
	if(curl_errno($ch))
	{
	    echo curl_error($ch);
		exit(2);
	}
	curl_close($ch);

	// $file = $g_has_error ? "result.txt" : "result." . $format;
	$file = $g_has_error ? "result.txt" : "result." . $format;
	file_put_contents($file, $data);

	// echo "\n$file saved successed, please open it \n";
	$baidu_tts_result_file_name= $file;
}


function read_header($ch, $header)
{
	global $g_has_error; 
	$comps = explode(":", $header);
	// 正常返回的头部 Content-Type: audio/*
	// 有错误的如 Content-Type: application/json
	if (count($comps) >= 2)
	{
		if (strcasecmp(trim($comps[0]), "Content-Type") == 0)
		{
			if (strpos($comps[1], "audio/") > 0 )
			{
				$g_has_error = false;
			}else
			{
				echo $header ." , has error \n";
			}
		}
	}
	return strlen($header);
}

?>