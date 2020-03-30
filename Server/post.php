<html> 
<body> 


</body> 
</html> 

<?php 
 
$id=$_GET["id"];

// echo $id;
// echo "**********************\r\n";

switch($id)
{
	case "wavdata":
        {
            //1. receive the wav binary data, and save to local, then convert to 16K wav data
            $file_w = fopen("receive.wav", 'w+');             
            $decodeData= base64_decode(file_get_contents('php://input'));           
            fwrite($file_w, $decodeData);
            fclose($file_w);
            // echo "save receive binary to tocal finish  \r\n";

            //2. conver the received data to 16K     
            // echo "shell_exec start  \r\n";  
            shell_exec("ffmpeg -y  -i  receive.wav  -acodec pcm_s16le -f s16le -ac 1 -ar 16000 receive16k.pcm");
            // echo "shell_exec end  \r\n";   

            //include_once("asr_raw.php");
            // echo "baidu start";
            include_once("baidu_speech.php");
            // echo "baidu end";
        }
        break;
		
	default:
		echo "who are you ? ";
		break;
}

?>

<?php
     
$filename="server_log.txt";
$myfile = fopen($filename, "a") or die("Unable to open file!");
$size = filesize($filename);

$requestIp=get_real_ip();
date_default_timezone_set('PRC');
fwrite($myfile, "\n******one time log start********\n");
fwrite($myfile, date('Y-m-d h:i:s', time()));
fwrite($myfile, "\n");
fwrite($myfile, "request ip is: : ".$requestIp);
fwrite($myfile, "\r\n");
fwrite($myfile, "log file size is:".$size );
//$txt = "Steve Jobs\n";
//fwrite($myfile, $txt);
fwrite($myfile, "\n******one time log end********\n");
fclose($myfile);


function get_real_ip($type = 0,$adv=false) {
    $type       =  $type ? 1 : 0;
    static $ip  =   NULL;
    if ($ip !== NULL) return $ip[$type];
    if($adv){
        if (isset($_SERVER['HTTP_X_FORWARDED_FOR'])) {
            $arr    =   explode(',', $_SERVER['HTTP_X_FORWARDED_FOR']);
            $pos    =   array_search('unknown',$arr);
            if(false !== $pos) unset($arr[$pos]);
            $ip     =   trim($arr[0]);
        }elseif (isset($_SERVER['HTTP_CLIENT_IP'])) {
            $ip     =   $_SERVER['HTTP_CLIENT_IP'];
        }elseif (isset($_SERVER['REMOTE_ADDR'])) {
            $ip     =   $_SERVER['REMOTE_ADDR'];
        }
    }elseif (isset($_SERVER['REMOTE_ADDR'])) {
        $ip     =   $_SERVER['REMOTE_ADDR'];
    }
    // IPµØÖ·ºÏ·¨ÑéÖ¤
    $long = sprintf("%u",ip2long($ip));
    $ip   = $long ? array($ip, $long) : array('0.0.0.0', 0);
    return $ip[$type];
}

?>

