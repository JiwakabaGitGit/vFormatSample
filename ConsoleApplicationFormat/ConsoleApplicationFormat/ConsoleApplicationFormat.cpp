// ConsoleApplicationFormat.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include <string>
#include <regex>
#include <vector>
#include <format>
#include <afx.h>
#include <afxwin.h>
#include <afxtempl.h>
#include <atlstr.h>
#include <iostream>
#include <Windows.h>
#include "yyjson.h"


enum class JSON_VALUE_TYPE
{
    TypeNull = YYJSON_TYPE_NULL,    // null
    TypeBool = YYJSON_TYPE_BOOL,    // true/false
    TypeNum  = YYJSON_TYPE_NUM,     // uint64_t/int64_t/double
    TypwStr  = YYJSON_TYPE_STR,     // string
    TypeArr  = YYJSON_TYPE_ARR,     // array
    TypeObj  = YYJSON_TYPE_OBJ,     // object
    TypeRaw  = YYJSON_TYPE_RAW,     // raw string
    TyeNone  = YYJSON_TYPE_NONE     // Invalid value
};


void CStringToWChar( CString & cstr, wchar_t * wstr, size_t size )
{
    // CString から wchar_t* へ変換
    wcsncpy_s( wstr, size, cstr.GetString(), _TRUNCATE );
}


wchar_t formatString[ 256 ] = { 0 };
wchar_t* myformat( CString format)
{
    memset( formatString, 0, sizeof( formatString ) );
    wchar_t wstr[ 256 ] = { 0 };
    CStringToWChar( format, wstr, sizeof( wstr ) / sizeof( wchar_t ) );

    wcsncpy_s( formatString, sizeof( formatString ) / sizeof( wchar_t ), wstr, _TRUNCATE );
    return formatString;
}


/// <summary>
/// CStringをstring型に変換する
/// </summary>
/// <param name="cstr">CString型の文字列</param>
/// <returns>string型の文字列</returns>
std::string CStringToString( const CString& cstr )
{
    CT2A asciiString( cstr );
    return std::string( asciiString );
}


/// <summary>
/// wstring型に変換する
/// </summary>
/// <param name="str">char型の文字列</param>
/// <returns>wstring型の文字列</returns>
std::wstring ConvertToWString( const char * str ) 
{
    int len = MultiByteToWideChar( CP_UTF8, 0, str, -1, nullptr, 0 );
    std::wstring wstr( len, L'\0' );
    MultiByteToWideChar( CP_UTF8, 0, str, -1, &wstr[ 0 ], len );

    return wstr;
}


/// <summary>
/// 文字列を数値に変換かチェックする
/// </summary>
/// <param name="str">数字文字列</param>
/// <param name="outValue">変換後の数値</param>
/// <returns></returns>
bool IsConvertibleToNumber ( const CString& str, int& outValue )
{
    // CStringAを使用してANSI文字列に変換
    CStringA strA ( str );
    const char * cstr = static_cast< const char * >( strA );

    // 変換後の数値を保持するための変数
    char * endptr = nullptr;
    long val = strtol ( cstr, &endptr, 10 );

    // 変換が成功したかどうかをチェック
    // 変換が失敗した場合、 endptr は元の文字列を指したままになる
    bool bRet = ( endptr != cstr && *endptr == '\0' );

    if( true == bRet )
    {
        // int型にキャストしてoutValueに格納
        outValue = static_cast< int >( val );
    }

    return bRet;
}


bool GetFormatedString( const CString src, const CStringArray& array, const yyjson_val* pRoot, CString& dst )
{
    bool bRet = true;
    int currentPos = 0;
    bool isFistFmt = true;              // 最初の{}のパースかどうか。最初意外は、順序指定の有無が異なった場合、エラーとする
    bool isOrderSpecification = false;  // {}内に順序指定あるかどうか
    int nKeyIndex = 0;

    while( currentPos < src.GetLength() )
    {
        int nStartPos= src.Find( '{', currentPos );
        if( -1 == nStartPos )
        {
            bRet = false;
            break;
        }
        int nEndPos = src.Find( '}', nStartPos + 1 );
        if( -1 == nEndPos )
        {
            bRet = false;
            break;
        }

        // {}から{}の間の文字列をコピー
        int len =nStartPos - currentPos;
        if( 0 < len )
        {
            CString mid = src.Mid( currentPos, len );
            dst += mid;
        }

        len = nEndPos - nStartPos + 1;
        CString strFmt = src.Mid( nStartPos, len );

        int colonPos = strFmt.Find( ':' );
        bool hasOrder = false;
        if( -1 < colonPos )
        {
            // コロンが{}の中に存在した場合は、左側に数字があるか確認する
            len = colonPos - 1;
            CString strNumber = strFmt.Mid( 1, len ).Trim();
            if( false == strNumber.IsEmpty() )
            {

                hasOrder = IsConvertibleToNumber( strNumber, nKeyIndex );
                if( false == hasOrder )
                {
                    // コロンの左側に文字があって、数字に変換できなければコンパイルエラー
                    bRet = false;
                    break;
                }
                // {}の中から順序指定を削除、{}の中にオプション書式を残す
                strFmt = '{' + strFmt.Mid( colonPos );
            }
        }
        else
        {
            len = nEndPos - nStartPos - 1;
            CString strNumber = strFmt.Mid( 1, len ).Trim();
            if( false == strNumber.IsEmpty() )
            {
                hasOrder = IsConvertibleToNumber( strNumber, nKeyIndex );
                if( false == hasOrder )
                {
                    // コロンの左側に文字があって、数字に変換できなければコンパイルエラー
                    bRet = false;
                    break;
                }
                // {}の中から順序指定を削除
                strFmt = "{}";
            }
        }

        if ( ( false == isFistFmt ) && ( isOrderSpecification != hasOrder ) )
        {
            // 順序指定の有無が最初の{}と異なった場合、コンパイルエラー
            // "{} {1}"、"{0} {}"のケースはエラー
            bRet = false;
            break;
        }

        if( ( true == isFistFmt ) && ( true == hasOrder ) )
        {
            isOrderSpecification = true;
        }

        if( ( true == isOrderSpecification ) && ( array.GetCount() <= nKeyIndex ) )
        {
            // 順序指定が範囲外
            // "BODY="{0},{1},{2}", fX,fY"のケースはエラー
            bRet = false;
            break;
        }

        CString strJsonKey = array.GetAt( nKeyIndex );


        dst += strJsonKey;

        if( false == isOrderSpecification )
        {
            // {}内の順序指定無しの場合の引数リストのインデックスをインクリメント
            nKeyIndex++;
        }
        currentPos = nEndPos + 1;
        isFistFmt = true;
    }
 
    return bRet;
}

int main()
{
    using namespace std;

    CString strFormat = "{ 0:<32 }, {2:+d}, { 1 }";
    CStringArray strKeyList;
    strKeyList.Add  ("fid");
    strKeyList.Add  ( "fx" );
    strKeyList.Add  ( "fy" );

    // JSONファイルのパス
    const char * filePath = "test.json";

    // JSONファイルを読み込む
    yyjson_doc * pDoc = yyjson_read_file( filePath, 0, NULL, NULL );
    if( !pDoc )
    {
        std::cerr << "JSONファイルの読み込みに失敗しました" << std::endl;
        return EXIT_FAILURE;
    }

    // ルートオブジェクトを取得
    yyjson_val* pRoot = yyjson_doc_get_root( pDoc );

    CString strDst = "";
    bool bRet = GetFormatedString ( strFormat, strKeyList, pRoot, strDst );
    std::wcout << strDst.GetString() << std::endl;


    // JSONオブジェクトを処理する例（適宜修正してください）
    if( yyjson_is_obj( pRoot ) )
    {
        yyjson_val * val = yyjson_obj_get( pRoot, "fid" );
        yyjson_type yType = yyjson_get_type( val );
        if( val && yyjson_is_str( val ) )
        {
            std::cout << "fid: " << yyjson_get_str( val ) << std::endl;
            try
            {
                std::wstring s1{ L"string1" };
                std::wstring s2{ L"string2" };
                const char* fid = yyjson_get_str( val );
                std::wstring s3 = ConvertToWString( fid );
                int nTest = 99;
                CString str = "1-{}\n2-{}\nfid-{}\n";
                std::wcout << std::vformat( myformat( str ), std::make_wformat_args( s1, s2, s3 ) ) << std::endl;
            }
            catch( const format_error & e )
            {
                cout << e.what() << endl;
            }
        }
    }

    //try
    //{
    //    std::wstring s1{ L"string1" };
    //    std::wstring s2{ L"string2" };
    //    CString str = "1-{}\n2-{}\nfid-{}\n";
    //    std::wcout << std::vformat( myformat( str ), std::make_wformat_args( s1, s2, 1 ) ) << std::endl;
    //}
    //catch( const format_error & e )
    //{
    //    cout << e.what() << endl;
    //}

    ////CString strFormat2 = "{}\n{}\n{}";
    //strDst = "";
    //bRet = GetFormatedString( strFormat2, strKeyList, pRoot, strDst );
    //std::wcout << strDst.GetString() << std::endl;



    // メモリを解放
    yyjson_doc_free( pDoc );

    return 0;
}

// プログラムの実行: Ctrl + F5 または [デバッグ] > [デバッグなしで開始] メニュー
// プログラムのデバッグ: F5 または [デバッグ] > [デバッグの開始] メニュー

// 作業を開始するためのヒント: 
//    1. ソリューション エクスプローラー ウィンドウを使用してファイルを追加/管理します 
//   2. チーム エクスプローラー ウィンドウを使用してソース管理に接続します
//   3. 出力ウィンドウを使用して、ビルド出力とその他のメッセージを表示します
//   4. エラー一覧ウィンドウを使用してエラーを表示します
//   5. [プロジェクト] > [新しい項目の追加] と移動して新しいコード ファイルを作成するか、[プロジェクト] > [既存の項目の追加] と移動して既存のコード ファイルをプロジェクトに追加します
//   6. 後ほどこのプロジェクトを再び開く場合、[ファイル] > [開く] > [プロジェクト] と移動して .sln ファイルを選択します
