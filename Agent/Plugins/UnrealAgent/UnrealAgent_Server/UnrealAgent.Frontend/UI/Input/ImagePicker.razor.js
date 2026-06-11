/**
 * 숨겨진 file input을 클릭하여 파일 다이얼로그를 엽니다.
 * 선택된 이미지를 Base64로 변환하여 C#에 전달합니다.
 */
export function pick(FileInputRef, dotNetRef)
{
    // 파일 선택시 호출되도록 함수 등록
    FileInputRef.onchange = function ()
    {
        var file = FileInputRef.files[0];
        if (!file) 
            return;

        // 읽으면 호출되도록 함수 등록
        var reader = new FileReader();
        reader.onload = function ()
        {
            var base64 = reader.result.split(",")[1];
            var mimeType = file.type || "image/png";
            dotNetRef.invokeMethodAsync("OnImagePicked", mimeType, base64);
        };
        
        // 파일 읽기 시작
        reader.readAsDataURL(file);

        FileInputRef.value = "";
    };
    
    // 파일 다이얼로그 활성화
    FileInputRef.click();
}