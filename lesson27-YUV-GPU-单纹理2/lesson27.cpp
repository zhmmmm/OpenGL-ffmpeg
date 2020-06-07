#include <windows.h>
#include <tchar.h>


#include "glew/glew.h"
#include "FFVideoReader.hpp"
#include "Thread.hpp"
#include "Timestamp.hpp"
#include "GLContext.h"
#include "CELLShader.hpp"
#include "CELLMath.hpp"



#define WM_UPDATE_VIDEO WM_USER + 100

void  getResourcePath(HINSTANCE hInstance,char pPath[1024])
{
    char    szPathName[1024];
    char    szDriver[64];
    char    szPath[1024];
    GetModuleFileNameA(hInstance,szPathName,sizeof(szPathName));
    _splitpath( szPathName, szDriver, szPath, 0, 0 );
    sprintf(pPath,"%s%s",szDriver,szPath);
}

class   DecodeThread :public Thread
{
public:
    FFVideoReader   _ffReader;
    HWND            _hWnd;

    bool            _exitFlag;
    Timestamp       _timestamp;
    GLContext       _glContext;
    unsigned        _textureYUV;
    PROGRAM_YUV1    _shaderTex;
public:
    DecodeThread()
    {
        _exitFlag   =   false;
        _hWnd       =   0;
    }

    virtual void    setup(HWND hwnd,const char* fileName = "11.flv")
    {
        _hWnd   =   hwnd;
        _ffReader.setup();
        _ffReader.load(fileName);
        _glContext.setup(hwnd,GetDC(hwnd));

        glewInit();

        glEnable(GL_TEXTURE_2D);

        _textureYUV =   createTexture(_ffReader._screenW,_ffReader._screenH + _ffReader._screenH/2);


        _shaderTex.initialize();
        
    }
    /**
    *   加载文件
    */
    virtual void    load(const char* fileName)
    {
        _ffReader.load(fileName);
    }
    virtual void    shutdown()
    {
        _exitFlag   =   true;
        Thread::join();
        _glContext.shutdown();
    }
    /**
    *   线程执行函数
    */
    virtual bool    run()
    {
        _timestamp.update();
        while(!_exitFlag)
        {
            FrameInfor*  infor  =   new FrameInfor();
            if (!_ffReader.readFrame(*infor))
            {
                break;
            }
            double      tims    =   infor->_pts * infor->_timeBase * 1000;
            //! 这里需要通知窗口进行重绘制更新，显示更新数据

            PostMessage(_hWnd,WM_UPDATE_VIDEO,(WPARAM)infor,0);
           
            
            double      elsped  =   _timestamp.getElapsedTimeInMilliSec();
            double      sleeps  =   (tims - elsped);
            if (sleeps > 1)
            {
                Sleep((DWORD)sleeps);
            }
        }

        return  true;
    }

    void    updateTexture(FrameInfor* infor)
    {
        glBindTexture(GL_TEXTURE_2D,_textureYUV);

        glTexSubImage2D(GL_TEXTURE_2D,0,    0,                      0,                      _ffReader._screenW,_ffReader._screenH,GL_ALPHA,GL_UNSIGNED_BYTE,infor->_data->data[0]);

        glTexSubImage2D(GL_TEXTURE_2D,0,    0,                      _ffReader._screenH,     _ffReader._screenW/2,_ffReader._screenH/2,GL_ALPHA,GL_UNSIGNED_BYTE,infor->_data->data[1]);

        glTexSubImage2D(GL_TEXTURE_2D,0,    _ffReader._screenW/2,   _ffReader._screenH,     _ffReader._screenW/2,_ffReader._screenH/2,GL_ALPHA,GL_UNSIGNED_BYTE,infor->_data->data[2]);

    }

    void    render()
    {
        struct  Vertex
        {
            CELL::float2  pos;
            CELL::float2  uvY;
            CELL::float2  uvU;
            CELL::float2  uvV;
        };

        RECT    rt;
        GetClientRect(_hWnd,&rt);
        int     w   =   rt.right - rt.left;
        int     h   =   rt.bottom - rt.top;
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(1,0,0,1);

        glViewport(0,0,w,h);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0,w,h,0,-100,100);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glBindTexture(GL_TEXTURE_2D,_textureYUV);

        float   yv  =   (float)_ffReader._screenH/(float)(_ffReader._screenH + _ffReader._screenH/2);

        float   fw   =   _ffReader._screenW;
        float   fh   =   _ffReader._screenH;

        float   x   =   0;
        float   y   =   0;

        float   Yv  =   (float)fh/(float)(fh + fh/2);

        float   Uu0 =   0;
        float   Uv0 =   Yv;
        float   Uu1 =   0.5f;
        float   Uv1 =   1.0f;

        float   Vu0 =   0.5f;
        float   Vv0 =   Yv;
        float   Vu1 =   1.0f;
        float   Vv1 =   1.0f;

        Vertex  vertex[]   =   
        {
            CELL::float2(x,y),          CELL::float2(0,0),      CELL::float2(Uu0,Uv0),   CELL::float2(Vu0,Vv0),
            CELL::float2(x + w,y),      CELL::float2(1,0),      CELL::float2(Uu1,Uv0),   CELL::float2(Vu1,Vv0),
            CELL::float2(x,y + h),      CELL::float2(0,Yv),     CELL::float2(Uu0,Uv1),   CELL::float2(Vu0,Vv1),  
            CELL::float2(x + w, y + h), CELL::float2(1,Yv),     CELL::float2(Uu1,Uv1),   CELL::float2(Vu1,Vv1),
        };

        _shaderTex.begin();

         CELL::matrix4   matMVP  =   CELL::ortho<float>(0,w,h,0,-100,100);

        glUniformMatrix4fv(_shaderTex._MVP, 1, false, matMVP.data());

        glUniform1i(_shaderTex._textureYUV, 0);
        glVertexAttribPointer(_shaderTex._position,2,GL_FLOAT,     false,  sizeof(Vertex),vertex);
        glVertexAttribPointer(_shaderTex._uvY,     2,GL_FLOAT,     false,  sizeof(Vertex),&vertex[0].uvY);
        glVertexAttribPointer(_shaderTex._uvU,     2,GL_FLOAT,     false,  sizeof(Vertex),&vertex[0].uvU);
        glVertexAttribPointer(_shaderTex._uvV,     2,GL_FLOAT,     false,  sizeof(Vertex),&vertex[0].uvV);
        glDrawArrays(GL_TRIANGLE_STRIP,0,4);


       
        _shaderTex.end();

        _glContext.swapBuffer();
    }

protected:

    unsigned    createTexture(int w,int h)
    {
        unsigned    texId;
        glGenTextures(1,&texId);
        glBindTexture(GL_TEXTURE_2D,texId);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D,0,GL_ALPHA,w,h,0,GL_ALPHA,GL_UNSIGNED_BYTE,0);

        return  texId;
    }

};

DecodeThread    g_decode;


LRESULT CALLBACK    windowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
    case WM_UPDATE_VIDEO:
        {
            FrameInfor* infor   =   (FrameInfor*)wParam;
            g_decode.updateTexture(infor);
            delete  infor;
            g_decode.render();
        }
        break;
    case WM_SIZE:
        break;
    case WM_CLOSE:
    case WM_DESTROY:
        g_decode.shutdown();
        PostQuitMessage(0);
        break;
    default:
        break;
    }

    return  DefWindowProc( hWnd, msg, wParam, lParam );
}

int     WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd )
{
    //  1   注册窗口类
    ::WNDCLASSEXA winClass;
    winClass.lpszClassName  =   "FFVideoPlayer";
    winClass.cbSize         =   sizeof(::WNDCLASSEX);
    winClass.style          =   CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
    winClass.lpfnWndProc    =   windowProc;
    winClass.hInstance      =   hInstance;
    winClass.hIcon	        =   0;
    winClass.hIconSm	    =   0;
    winClass.hCursor        =   LoadCursor(NULL, IDC_ARROW);
    winClass.hbrBackground  =   (HBRUSH)(BLACK_BRUSH);
    winClass.lpszMenuName   =   NULL;
    winClass.cbClsExtra     =   0;
    winClass.cbWndExtra     =   0;
    RegisterClassExA(&winClass);

    //  2 创建窗口
    HWND    hWnd   =   CreateWindowExA(
        NULL,
        "FFVideoPlayer",
        "FFVideoPlayer",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0,
        0,
        480,
        320, 
        0, 
        0,
        hInstance, 
        0
        );

    UpdateWindow( hWnd );
    ShowWindow(hWnd,SW_SHOW);


    char    szPath[1024];
    char    szPathName[1024];

    getResourcePath(hInstance,szPath);

    sprintf(szPathName,"%sdata/11.flv",szPath);

    g_decode.setup(hWnd,szPathName);
    g_decode.start();

    MSG     msg =   {0};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    g_decode.shutdown();
    return  0;
}