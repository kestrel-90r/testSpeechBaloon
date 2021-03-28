﻿
# include <Siv3D.hpp> // OpenSiv3D v0.6
# include <Siv3D/EngineLog.hpp>
# include <Siv3D/Random.hpp>
# include "Siv3D/Geometry2D.hpp"
# include "Siv3D/SimpleGUI.hpp"

Shape2D SpeechBalloon1( const Float2& center ,                  // 中心座標
                        const uint16& ngon = 6,                 // 基準図形の種類
                        const  int32& angle = 0,                // 基準図形の角度
                        const uint32& size = 150,               // 基準図形の大きさ
                        const  int32& rotate = 0,               // 雲の回転(アフィン変換)
                        const Float2& anchor = Float2{ 0,0 },   // 吹出し調整点の座標(中心座標からの相対位置)
                        const Float2& scale = Float2{ 1,1 },    // 雲の拡縮(アフィン変換)
                        const Float2& stretch = Float2{ 1,1 },  // 引伸ばし倍率
                        const Float2& divide = Float2{ 0,0 },   // 分割点座標(中心座標からの相対位置)
                        const Float2& target = Float2{ 0,0 } )  // 吹出し目標点の座標(中心座標からの相対位置)
{
    Float2 div = center + divide;   // 原点から相対指定
    Float2 anc = center + anchor;
    Float2 trg = center + target;
    Float2 str = stretch;
    if (str.x < 1) str.x = 1;       // 引伸しは縮小に対応しない
    if (str.y < 1) str.y = 1;

    //基準多角形
    Array<Float2> vertices;
    for (uint32 i = 0; i < 360; i += 360 / ngon)
    {
        vertices << center + Float2( Math::Cos(ToRadians(i+angle)), 
                                     Math::Sin(ToRadians(i+angle))) * size;

    }

    //テスト用
    for (auto v : vertices)Circle(v, 4).drawFrame(1,Palette::Green);

    //基準引伸し
    uint32 isStretch = (str == Float2(1,1)) ? 0 : 1;
    if (isStretch)
    {
        Float2 stretched = str / Float2(1,1);
        Float2 resized = size * stretched - Float2(size,size);  //引き伸ばし後サイズの差分
        for (auto& vt : vertices)
        {
            if      (vt.x <  div.x && vt.y < div.y)  { vt.x = vt.x - resized.x;vt.y = vt.y - resized.y; }
            else if (vt.x >= div.x && vt.y < div.y)  { vt.x = vt.x + resized.x;vt.y = vt.y - resized.y; }
            else if (vt.x <  div.x && vt.y >= div.y) { vt.x = vt.x - resized.x;vt.y = vt.y + resized.y; }
            else if (vt.x >= div.x && vt.y >= div.y) { vt.x = vt.x + resized.x;vt.y = vt.y + resized.y; }
        }
    }

    //アフィン変換
    Mat3x2 mat32 = Mat3x2::Identity().scaled(scale, center).rotated(ToRadians(rotate), center);
    for (auto& vt : vertices) vt = mat32.transformPoint(vt);

    //吹出し有無(原点＝吹出の場合キャンセル)
    uint32 isTarget = (center == trg) ? 0 : 2;   
    if (isTarget)
    {
        vertices << anc;
        vertices << trg;
    }

    //基準多角形を登録
    Array<TriangleIndex> indices(ngon - 2 + isTarget);
    TriangleIndex* pdst = indices.data();
    for (uint32 i=1; i < ngon-1+isTarget ; ++i)
    {
        pdst->i0 = 0;
        pdst->i1 = i;
        pdst->i2 = i + 1;
        ++pdst;
    }

    return{ std::move(vertices), std::move(indices) };
}


enum PARAMID { NGON,ANGLE,SIZE,NSUB,DETAIL,BURST,WIRE,
               ORGX,ORGY,ANCX,ANCY,TRGX,TRGY,ROTATE,
               STRX,STRY,SCAX,SCAY,DIVX,DIVY,
               M_ORG,M_ANC, M_TRG, M_DIV,
               NUMGUI };

enum GRABAREA { RELEASE, G_ORG, G_ANC, G_TRG, G_DIV };

struct GRABSTATE
{
    GRABAREA area ;
    int32 value ;
} ;

Rect guiRects[NUMGUI] = {};

template <typename V,typename T> 
V guiValue(const Font &font,const String &text,Rect &guirect,double wheel, V initvalue, V value,T min,T max)
{
    if ( guirect.mouseOver() && wheel != 0) value = Math::Clamp(value+wheel,min,max); 
    if ( guirect.leftClicked()) value = initvalue;
    font(text).draw(guirect, guirect.mouseOver() ? ColorF(1) : ColorF(0)); 
    return value;
}

template <typename V> 
V guiDrag(const Font &font,const String &text, Rect &guirect,Vec2 delta,GRABSTATE &grabstate, GRABAREA garea, 
          ColorF activecolor,ColorF inactivecolor, V value )
{
    font(text).draw(Arg::center(value),guirect.mouseOver() ? activecolor : inactivecolor );
    if (grabstate.area == RELEASE && guirect.leftClicked()) grabstate.area = garea;
    if (grabstate.area == garea) { value += Vec2(Cursor::Delta()); guirect.x = value.x; guirect.y = value.y; }
    return value;
}

template <typename V>
void guiSyncMarker( PARAMID m_id, V origin, V value )
{
    static Float2 before[NUMGUI];
    if (value != before[m_id])
    {
        if (before[m_id] == Float2{ 0, 0 }) before[m_id] = value;

        Float2 mv = origin - value;
        before[m_id] = value;
        guiRects[m_id].x = origin.x + value.x;
        guiRects[m_id].y = origin.y + value.y;
    }
}

void Main()
{
    Window::SetStyle(WindowStyle::Sizable);         // ウィンドウを手動リサイズ可能にする
    Scene::SetBackground(ColorF(0.8, 0.9, 1.0));    // ウィンドウサイズに合わせて拡縮

    const Font fontM(50);                           // 各種フォント初期化
    const Font fontS(15);
    const Font iconM(30,Typeface::Icon_MaterialDesign);

    Float2 stretch { 1, 1 };
    Float2 scale { 1, 1 };
    Float2 origin { 300, 200 };
    Float2 anchor { +120, -20 };
    Float2 target { +250, -20 };
    Float2 divide {  +10, +10 };

    uint32 ngon = 6;
    int32  angle = 0;
    uint32 size = 130;
    int32  rotate = 0;

    uint32 nsubngon = 40;
    int32 detail = -10;
    float burst = 1.0;
    uint8 wire = 1;

    GRABSTATE grabState { RELEASE,0 };              // テスト(パラメータ)初期化
    uint32 fs = fontS.fontSize() + 8;
    Rect rc = Rect(5, Scene::Height()-100, fs*5,fs);

    for (uint32 i = 0; i < NUMGUI; i++)             // テスト(マーカー)初期化
    {
        if      (i == M_ORG) { guiRects[i] = Rect(origin.x - 10, origin.y - 10, 30, 30); }
        else if (i == M_ANC) { guiRects[i] = Rect(origin.x + anchor.x - 10, origin.y + anchor.y - 10, 30, 30); }
        else if (i == M_TRG) { guiRects[i] = Rect(origin.x + target.x - 10, origin.y + target.y - 10, 30, 30); }
        else if (i == M_DIV) { guiRects[i] = Rect(origin.x + divide.x - 10, origin.y + divide.y - 10, 30, 30); }
        else
        {
            guiRects[i] = rc;
            rc.x += fs * 5;
            if (rc.x > Scene::Width() - 20) { rc.x = 5; rc.y += fs; }
        }
    }


    while (System::Update())                        // メインループ
    {
        String text = U"Speech\nBaloon";
        RectF rectf = RectF{ origin.x, origin.y, fontM.fontSize()*text.size()/3, fontM.height()*2 };

        // 吹き出し形状テスト ※線描画で表示を選択
        Shape2D shape = SpeechBalloon1(origin, ngon, angle, size, rotate, anchor, scale, stretch, divide, target);
        if      (wire == 0) shape.draw(Palette::Bisque);
        else if (wire == 1) shape.draw(Palette::Bisque).drawFrame(2, Palette::Black);
        else if (wire == 2) shape.draw(Palette::Bisque).drawWireframe(2, Palette::Black);
        else if (wire == 3) shape.drawWireframe(2, Palette::Black);

        // SpeechBaloon文字列
        fontM(text).draw(Arg::center(Vec2(origin.x, origin.y)), ColorF(0));

        // テスト用UI(パラメータ) ※マウスオーバーのウィール操作で値変更、左クリックで初期値
        double wheel = Mouse::Wheel();
        ngon = guiValue( fontS, U"多角形:{}"_fmt(ngon), guiRects[NGON], wheel, (uint32)6, ngon, 3, 256);
        angle = guiValue( fontS, U"角度:{}"_fmt(angle), guiRects[ANGLE], wheel, (int32)0, angle, -360, 720);
        size = guiValue( fontS, U"大きさ:{}"_fmt(size), guiRects[SIZE], wheel, (uint32)140, size, 10, 400);

        nsubngon = guiValue( fontS, U"分割A:{}"_fmt(nsubngon), guiRects[NSUB], wheel, (uint32)40, nsubngon, 4, 100);
        detail = guiValue( fontS, U"詳細:{}"_fmt(detail), guiRects[DETAIL], wheel, (int32)10, detail, -100, 100);
        burst = guiValue( fontS, U"連続:{}"_fmt(burst), guiRects[BURST], wheel/10, (float)1.4, burst, 0, 100);
        wire = guiValue( fontS, U"線描画:{}"_fmt(wire), guiRects[WIRE], wheel, (uint8)0, wire, 0, 3);

        origin.x = guiValue( fontS, U"原点X:{}"_fmt(origin.x), guiRects[ORGX], wheel, (float)200, origin.x, 0, Scene::Width());
        origin.y = guiValue( fontS, U"原点Y:{}"_fmt(origin.y), guiRects[ORGY], wheel, (float)200, origin.y, 0, Scene::Height());
        anchor.x = guiValue( fontS, U"アンカーX:{}"_fmt(anchor.x), guiRects[ANCX], wheel, (float)+120, anchor.x, -1000, +1000);
        anchor.y = guiValue( fontS, U"アンカーY:{}"_fmt(anchor.y), guiRects[ANCY], wheel, (float)-20, anchor.y, -1000, +1000);
        target.x = guiValue( fontS, U"吹出X:{}"_fmt(target.x), guiRects[TRGX], wheel, (float)+250, target.x, -1000, +1000);
        target.y = guiValue( fontS, U"吹出Y:{}"_fmt(target.y), guiRects[TRGY], wheel, (float)-20, target.y, -1000, +1000);
        rotate = guiValue(fontS, U"回転:{}"_fmt(rotate), guiRects[ROTATE], wheel, (int32)0, rotate, -360, 720);
        stretch.x = guiValue( fontS, U"引伸X:{}"_fmt(stretch.x), guiRects[STRX], wheel/10, (float)1, stretch.x, 0, 10);
        stretch.y = guiValue( fontS, U"引伸Y:{}"_fmt(stretch.y), guiRects[STRY], wheel/10, (float)1, stretch.y, 0, 10);
        scale.x = guiValue( fontS, U"拡縮X:{}"_fmt(scale.x), guiRects[SCAX], wheel/10, (float)1, scale.x, 0, 10);
        scale.y = guiValue( fontS, U"拡縮Y:{}"_fmt(scale.y), guiRects[SCAY], wheel/10, (float)1, scale.y, 0, 10);
        divide.x = guiValue(fontS, U"分割X:{}"_fmt(divide.x), guiRects[DIVX], wheel, (float)0, divide.x, -1000, +1000);
        divide.y = guiValue(fontS, U"分割Y:{}"_fmt(divide.y), guiRects[DIVY], wheel, (float)0, divide.y, -1000, +1000);

        // テスト用UI(マーカー) ※マウスオーバーでドラッグ
        origin = guiDrag(iconM,U"\xF1382", guiRects[M_ORG],Vec2(Cursor::Delta()),grabState,G_ORG,ColorF(0,0,1),ColorF(1),origin );

        {
            static Float2 before;
            if (origin != before)
            {
                if (before == Float2{ 0, 0 }) before = origin;

                Float2 mv = origin - origin;
                before = origin;
                guiRects[M_ANC].x = origin.x + anchor.x; guiRects[M_ANC].y = origin.y + anchor.y;
                guiRects[M_TRG].x = origin.x + target.x; guiRects[M_TRG].y = origin.y + target.y;
                guiRects[M_DIV].x = origin.x + divide.x; guiRects[M_DIV].y = origin.y + divide.y;
            }
        }

        guiSyncMarker(M_ANC, origin, anchor);
        guiSyncMarker(M_TRG, origin, target);
        guiSyncMarker(M_DIV, origin, divide);

        Float2 oa = origin + anchor;
        Float2 ot = origin + target;
        Float2 od = origin + divide;
        anchor = guiDrag(iconM, U"\xF0031", guiRects[M_ANC],Vec2(Cursor::Delta()),grabState,G_ANC,ColorF(1,0,0),ColorF(1),oa );
        target = guiDrag(iconM, U"\xF04FE", guiRects[M_TRG], Vec2(Cursor::Delta()), grabState, G_TRG, ColorF(1, 0, 1), ColorF(1), ot);
        divide = guiDrag(iconM, U"\xF0556", guiRects[M_DIV], Vec2(Cursor::Delta()), grabState, G_DIV, ColorF(1, 1, 0), ColorF(1), od);
        anchor -= origin;               // 原点相対座標
        target -= origin;
        divide -= origin;

//        SimpleGUI::TextBoxRegion(Vec2(50, 560),700);

        if (MouseL.up()) grabState.area = RELEASE;
    }
}
