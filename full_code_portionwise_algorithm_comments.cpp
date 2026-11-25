#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __APPLE__
#include <GLUT/glut.h>
#include <OpenGL/glu.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
#endif

#include <cmath>

/*
This file contains algorithm comments placed directly above the code portions
where each algorithm/technique is actually used.
Main algorithms/techniques annotated below include:
- Linear Interpolation (LERP)
- Clamping and Smoothstep easing
- Cubic Bezier curve evaluation
- Bezier tangent/normal based offset geometry
- Parametric animation
- Perspective scaling
- Sin/Cos based oscillation
- Cyclic wrapping / looping
- Region clipping (scissor test)
- Layered rendering / painter-style draw order
*/

float trainX = -420.0f;
bool trainMoving = true;
bool carsMoving = true;
bool boatMoving = true;
float cloudOffset = 0.0f;
float treeSwayPhase = 0.0f;
float boatT = 0.18f;
float boatSpeed = 0.0016f;
bool boatForward = true;
bool isNight = false;
float starTwinklePhase = 0.0f;
float riverAnim = 0.0f;

// flyover vehicles on two lanes
float flyoverRightLaneX[3] = {-220.0f, 320.0f, 860.0f};
float flyoverLeftLaneX[3]  = {1460.0f, 980.0f, 520.0f};

// cars on turned bottom road
float bottomCarsT[5] = {0.05f, 0.23f, 0.41f, 0.59f, 0.77f};

const float PI = 3.1415926f;

// ------------------------------
// Turned road control
// ------------------------------
const float VROAD_END_Y = 565.0f;
const float HROAD_END_X = 1440.0f;
const float HROAD_CENTER_Y = 565.0f;

// smooth turn region
const float VROAD_STRAIGHT_END_Y = 515.0f;
const float HROAD_STRAIGHT_START_X = 470.0f;

// narrower road after turn
const float TURN_START_WIDTH = 52.0f;
const float TURN_END_WIDTH   = 22.0f;

// lane / shoulder feel
const float SIDE_LINE_MARGIN = 3.5f;
const float CAR_LANE_RATIO_VERTICAL = 0.10f;
const float CAR_LANE_RATIO_CURVE    = 0.08f;
const float CAR_LANE_RATIO_HORIZONTAL = 0.07f;

struct Vec2
{
    float x, y;
};

Vec2 getBoatPosition(float t);
float getBoatScale(float t);
float getBoatAngle(float t);

void setColor(unsigned char r, unsigned char g, unsigned char b)
{
    glColor3ub(r, g, b);
}

void drawFilledRect(float x1, float y1, float x2, float y2)
{
    glBegin(GL_QUADS);
    glVertex2f(x1, y1);
    glVertex2f(x2, y1);
    glVertex2f(x2, y2);
    glVertex2f(x1, y2);
    glEnd();
}

void drawPolygon(float pts[][2], int n)
{
    glBegin(GL_POLYGON);
    for (int i = 0; i < n; i++)
        glVertex2f(pts[i][0], pts[i][1]);
    glEnd();
}

void drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3)
{
    glBegin(GL_TRIANGLES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glVertex2f(x3, y3);
    glEnd();
}

/*
Algorithm: Polygon approximation of a circle
Reason: Multiple vertices are generated using cos(theta), sin(theta) and connected
        as a filled polygon to approximate a smooth circle.
*/
void drawCircle(float cx, float cy, float r, int segments = 80)
{
    glBegin(GL_POLYGON);
    for (int i = 0; i < segments; i++)
    {
        float ang = 2.0f * PI * i / segments;
        glVertex2f(cx + cos(ang) * r, cy + sin(ang) * r);
    }
    glEnd();
}

/*
Algorithm: Polygon approximation of an ellipse
Reason: Parametric ellipse points are generated using trigonometric functions
        and drawn as a filled polygon.
*/
void drawEllipse(float cx, float cy, float rx, float ry, int segments = 80)
{
    glBegin(GL_POLYGON);
    for (int i = 0; i < segments; i++)
    {
        float ang = 2.0f * PI * i / segments;
        glVertex2f(cx + cos(ang) * rx, cy + sin(ang) * ry);
    }
    glEnd();
}

void drawLine(float x1, float y1, float x2, float y2, float width = 1.0f)
{
    glLineWidth(width);
    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}


/*
Technique: Clipping / Scissor Test
Reason: Rendering is limited to a rectangular region so light/glow effects stay
        inside their intended lane or road area.
*/
void beginClipRect(int x, int y, int w, int h)
{
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, w, h);
}

void endClipRect()
{
    glDisable(GL_SCISSOR_TEST);
}

void drawHeadlightGlow(float x, float y, float outerR, float innerR,
                       unsigned char r = 255, unsigned char g = 240, unsigned char b = 180)
{
    if (!isNight) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4ub(r, g, b, 52);
    drawCircle(x, y, outerR, 28);

    glColor4ub(r, g, b, 92);
    drawCircle(x, y, innerR, 24);

    glDisable(GL_BLEND);
}

void drawHeadlightBulb(float x, float y, float r)
{
    setColor(255, 245, 185);
    drawCircle(x, y, r, 20);
}


void drawHouseFrontLight(float x, float y, float outerR = 28.0f, float innerR = 14.0f)
{
    if (!isNight) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4ub(255, 235, 170, 48);
    drawCircle(x, y, outerR, 34);

    glColor4ub(255, 242, 190, 88);
    drawCircle(x, y, innerR, 28);

    // soft ground spill below the bulb
    glColor4ub(255, 228, 155, 42);
    drawEllipse(x, y - outerR * 0.9f, outerR * 1.15f, outerR * 0.42f, 36);

    glDisable(GL_BLEND);

    setColor(255, 245, 188);
    drawCircle(x, y, 3.6f, 20);
}

void drawTrainHeadlightsOverlay(float x, float y, float s)
{
    drawHeadlightGlow(x + 218 * s, y + 24 * s, 12.0f * s, 6.5f * s);
    drawHeadlightGlow(x + 218 * s, y + 36 * s, 12.0f * s, 6.5f * s);
    drawHeadlightBulb(x + 218 * s, y + 24 * s, 2.6f * s);
    drawHeadlightBulb(x + 218 * s, y + 36 * s, 2.6f * s);
}

void drawCarLightsOverlay(float x, float y, float s, bool right)
{
    if (right)
    {
        drawHeadlightGlow(x + 111 * s, y + 16 * s, 10.0f * s, 5.0f * s);
        drawHeadlightBulb(x + 111 * s, y + 16 * s, 2.0f * s);
    }
    else
    {
        drawHeadlightGlow(x + 3 * s, y + 16 * s, 10.0f * s, 5.0f * s);
        drawHeadlightBulb(x + 3 * s, y + 16 * s, 2.0f * s);
    }
}

void drawBusLightsOverlay(float x, float y, float s, bool right)
{
    if (right)
    {
        drawHeadlightGlow(x + 135 * s, y + 18 * s, 11.0f * s, 5.8f * s);
        drawHeadlightGlow(x + 135 * s, y + 25 * s, 9.5f * s, 5.0f * s);
        drawHeadlightBulb(x + 135 * s, y + 18 * s, 2.2f * s);
        drawHeadlightBulb(x + 135 * s, y + 25 * s, 2.0f * s);
    }
    else
    {
        drawHeadlightGlow(x + 3 * s, y + 18 * s, 11.0f * s, 5.8f * s);
        drawHeadlightGlow(x + 3 * s, y + 25 * s, 9.5f * s, 5.0f * s);
        drawHeadlightBulb(x + 3 * s, y + 18 * s, 2.2f * s);
        drawHeadlightBulb(x + 3 * s, y + 25 * s, 2.0f * s);
    }
}

void drawTruckLightsOverlay(float x, float y, float s, bool right)
{
    if (right)
    {
        drawHeadlightGlow(x + 4 * s, y + 18 * s, 10.5f * s, 5.5f * s);
        drawHeadlightBulb(x + 4 * s, y + 18 * s, 2.2f * s);
    }
    else
    {
        drawHeadlightGlow(x + 135 * s, y + 18 * s, 10.5f * s, 5.5f * s);
        drawHeadlightBulb(x + 135 * s, y + 18 * s, 2.2f * s);
    }
}

void drawTopViewCarLightsOverlay(float x, float y, float s, float angleDeg)
{
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glRotatef(angleDeg, 0.0f, 0.0f, 1.0f);

    drawHeadlightGlow(-4.0f * s, 22.0f * s, 9.0f * s, 4.0f * s);
    drawHeadlightGlow( 4.0f * s, 22.0f * s, 9.0f * s, 4.0f * s);
    drawHeadlightBulb(-4.0f * s, 19.7f * s, 1.2f * s);
    drawHeadlightBulb( 4.0f * s, 19.7f * s, 1.2f * s);

    glPopMatrix();
}

/*
Algorithm: Linear Interpolation (LERP)
Formula: a + (b - a) * t
Use: Smoothly transitions between two values for position, width, scaling, etc.
*/
float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

/*
Algorithm: Clamping
Use: Restricts a value to the range [0, 1] so animation and interpolation
     parameters do not exceed valid bounds.
*/
float clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

/*
Algorithm: Smoothstep easing
Formula: x*x*(3 - 2*x)
Use: Creates a smoother ease-in/ease-out transition than plain linear movement.
*/
float smoothstep01(float value)
{
    value = clamp01(value);
    return value * value * (3.0f - 2.0f * value);
}

/*
Algorithm: Phase wrapping / cyclic reset
Use: Keeps continuously increasing animation phase values inside a fixed range
     so repeating motion can loop forever without overflow.
*/
void wrapPhase(float &value, float limit)
{
    if (value > limit)
        value -= limit;
}

// perspective road helpers
/*
Technique: Perspective interpolation
Use: Road center shifts gradually with depth to create a perspective view.
*/
float perspectiveRoadCenterX(float y)
{
    return lerp(425.0f, 420.0f, y / 600.0f);
}

/*
Technique: Perspective scaling
Use: Road width becomes smaller for far distance to simulate depth.
*/
float perspectiveRoadWidth(float y)
{
    return lerp(220.0f, 68.0f, y / 600.0f);
}

float turnedRoadWidth(float x)
{
    float t = (x - HROAD_STRAIGHT_START_X) / (HROAD_END_X - HROAD_STRAIGHT_START_X);
    return lerp(TURN_START_WIDTH, TURN_END_WIDTH, clamp01(t));
}

Vec2 makeVec(float x, float y)
{
    Vec2 v;
    v.x = x;
    v.y = y;
    return v;
}

Vec2 addVec(Vec2 a, Vec2 b)
{
    return makeVec(a.x + b.x, a.y + b.y);
}

Vec2 subVec(Vec2 a, Vec2 b)
{
    return makeVec(a.x - b.x, a.y - b.y);
}

Vec2 mulVec(Vec2 a, float s)
{
    return makeVec(a.x * s, a.y * s);
}

float lenVec(Vec2 a)
{
    return sqrt(a.x * a.x + a.y * a.y);
}

/*
Algorithm: Vector normalization
Use: Converts any vector into a unit vector so it can safely represent only
     direction (used for tangent and normal computation).
*/
Vec2 normalizeVec(Vec2 a)
{
    float l = lenVec(a);
    if (l < 0.0001f) return makeVec(0.0f, 0.0f);
    return makeVec(a.x / l, a.y / l);
}

/*
Algorithm: Sinusoidal oscillation
Use: Uses sine wave motion to create natural back-and-forth tree sway.
*/
float getTreeSwayDeg(float x, float y)
{
    return sin(treeSwayPhase + x * 0.018f + y * 0.011f) * 3.2f;
}

/*
Algorithm: Position wrapping / endless scrolling
Use: Clouds that move beyond screen bounds are wrapped back to the other side
     for continuous sky animation.
*/
float getWrappedCloudX(float baseX)
{
    float x = baseX + cloudOffset;
    while (x > 1520.0f) x -= 1820.0f;
    return x;
}

// ------------------------------
// Smooth curve centerline (Bezier)
// ------------------------------
Vec2 turnP0()
{
    return makeVec(perspectiveRoadCenterX(VROAD_STRAIGHT_END_Y), VROAD_STRAIGHT_END_Y);
}

Vec2 turnP1()
{
    Vec2 p0 = turnP0();
    return makeVec(p0.x, 548.0f);
}

Vec2 turnP2()
{
    return makeVec(438.0f, HROAD_CENTER_Y);
}

Vec2 turnP3()
{
    return makeVec(HROAD_STRAIGHT_START_X, HROAD_CENTER_Y);
}

/*
Algorithm: Cubic Bezier Curve Evaluation
Use: Computes a smooth point on the turned road centerline using four control
     points and Bernstein polynomial blending.
*/
Vec2 bezierPoint(float t)
{
    Vec2 p0 = turnP0();
    Vec2 p1 = turnP1();
    Vec2 p2 = turnP2();
    Vec2 p3 = turnP3();

    float u = 1.0f - t;
    float b0 = u * u * u;
    float b1 = 3.0f * u * u * t;
    float b2 = 3.0f * u * t * t;
    float b3 = t * t * t;

    return makeVec(
        b0 * p0.x + b1 * p1.x + b2 * p2.x + b3 * p3.x,
        b0 * p0.y + b1 * p1.y + b2 * p2.y + b3 * p3.y
    );
}

/*
Algorithm: Derivative of Cubic Bezier Curve
Use: Computes tangent direction of the curve, which is later used to find
     rotation angle, road edges, normals, and lane offsets.
*/
Vec2 bezierTangent(float t)
{
    Vec2 p0 = turnP0();
    Vec2 p1 = turnP1();
    Vec2 p2 = turnP2();
    Vec2 p3 = turnP3();

    float u = 1.0f - t;

    Vec2 a = mulVec(subVec(p1, p0), 3.0f * u * u);
    Vec2 b = mulVec(subVec(p2, p1), 6.0f * u * t);
    Vec2 c = mulVec(subVec(p3, p2), 3.0f * t * t);

    return addVec(addVec(a, b), c);
}

float turnCurveWidth(float t)
{
    float startW = perspectiveRoadWidth(VROAD_STRAIGHT_END_Y);
    return lerp(startW, TURN_START_WIDTH, t);
}

/*
Algorithm: Curve ribbon generation using tangent-normal geometry
Use: Builds the road surface as a triangle strip around the Bezier centerline.
     Tangent gives direction, normal gives left/right expansion.
*/
void drawBezierRoadBand(int segments = 80)
{
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= segments; i++)
    {
        float t = (float)i / (float)segments;

        Vec2 p = bezierPoint(t);
        Vec2 tan = normalizeVec(bezierTangent(t));
        Vec2 n = makeVec(-tan.y, tan.x);

        float w = turnCurveWidth(t);

        Vec2 left = addVec(p, mulVec(n, w * 0.5f));
        Vec2 right = subVec(p, mulVec(n, w * 0.5f));

        glVertex2f(left.x, left.y);
        glVertex2f(right.x, right.y);
    }
    glEnd();
}

/*
Algorithm: Offset curve generation
Use: Draws left/right road boundaries by moving from the Bezier centerline
     along its normal vector.
*/
void drawBezierEdge(bool leftEdge, float width = 1.0f, int segments = 80)
{
    glLineWidth(width);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= segments; i++)
    {
        float t = (float)i / (float)segments;

        Vec2 p = bezierPoint(t);
        Vec2 tan = normalizeVec(bezierTangent(t));
        Vec2 n = makeVec(-tan.y, tan.x);

        float w = turnCurveWidth(t);

        Vec2 edge = leftEdge ? addVec(p, mulVec(n, w * 0.5f))
                             : subVec(p, mulVec(n, w * 0.5f));

        glVertex2f(edge.x, edge.y);
    }
    glEnd();
}

/*
Algorithm: Parallel curve / normal-offset line
Use: Creates guard rails, shoulder hints, and other lines parallel to the road
     by shifting points along the normal direction.
*/
void drawBezierOffsetLine(float offset, float width = 1.0f, int segments = 80)
{
    glLineWidth(width);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= segments; i++)
    {
        float t = (float)i / (float)segments;
        Vec2 p = bezierPoint(t);
        Vec2 tan = normalizeVec(bezierTangent(t));
        Vec2 n = makeVec(-tan.y, tan.x);
        Vec2 q = addVec(p, mulVec(n, offset));
        glVertex2f(q.x, q.y);
    }
    glEnd();
}

void drawBezierRibbonShadow(float offsetX, float offsetY, float extraWidth, int segments = 80)
{
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= segments; i++)
    {
        float t = (float)i / (float)segments;

        Vec2 p = bezierPoint(t);
        p.x += offsetX;
        p.y += offsetY;

        Vec2 tan = normalizeVec(bezierTangent(t));
        Vec2 n = makeVec(-tan.y, tan.x);

        float w = turnCurveWidth(t) + extraWidth;

        Vec2 left = addVec(p, mulVec(n, w * 0.5f));
        Vec2 right = subVec(p, mulVec(n, w * 0.5f));

        glVertex2f(left.x, left.y);
        glVertex2f(right.x, right.y);
    }
    glEnd();
}

void drawRoundTree(float x, float y, float s)
{
    setColor(141, 90, 43);
    drawFilledRect(x - 4 * s, y, x + 4 * s, y + 24 * s);

    glPushMatrix();
    glTranslatef(x, y + 24 * s, 0.0f);
    glRotatef(getTreeSwayDeg(x, y), 0.0f, 0.0f, 1.0f);

    setColor(43, 160, 54);
    drawCircle(0, 10 * s, 14 * s);
    drawCircle(-10 * s, 8 * s, 10 * s);
    drawCircle(10 * s, 8 * s, 10 * s);
    drawCircle(0, 18 * s, 10 * s);

    glPopMatrix();
}

void drawChristmasTree(float x, float y, float s)
{
    setColor(120, 78, 40);
    drawFilledRect(x - 4 * s, y, x + 4 * s, y + 20 * s);

    glPushMatrix();
    glTranslatef(x, y + 18 * s, 0.0f);
    glRotatef(getTreeSwayDeg(x, y) * 0.9f, 0.0f, 0.0f, 1.0f);

    setColor(20, 120, 42);
    drawTriangle(0, 42 * s, -22 * s, 10 * s, 22 * s, 10 * s);
    drawTriangle(0, 28 * s, -26 * s, -2 * s, 26 * s, -2 * s);
    drawTriangle(0, 14 * s, -30 * s, -14 * s, 30 * s, -14 * s);

    setColor(245, 215, 70);
    drawCircle(0, 47 * s, 3.0f * s, 24);

    setColor(220, 45, 45);
    drawCircle(-10 * s, 25 * s, 2.2f * s, 20);
    drawCircle(11 * s, 16 * s, 2.2f * s, 20);

    setColor(65, 160, 255);
    drawCircle(9 * s, 31 * s, 2.0f * s, 20);
    drawCircle(-14 * s, 6 * s, 2.0f * s, 20);

    setColor(255, 190, 40);
    drawCircle(15 * s, 1 * s, 2.0f * s, 20);
    drawCircle(-2 * s, 12 * s, 1.8f * s, 20);

    glPopMatrix();
}

void drawWindow(float x1, float y1, float x2, float y2)
{
    setColor(70, 86, 104);
    drawFilledRect(x1, y1, x2, y2);

    if (isNight)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // warm room light inside the window
        glColor4ub(255, 214, 120, 235);
        drawFilledRect(x1 + 2, y1 + 2, x2 - 2, y2 - 2);

        glColor4ub(255, 236, 170, 90);
        drawFilledRect(x1 + 3, y1 + 3, x2 - 3, y2 - 3);

        glDisable(GL_BLEND);
    }
    else
    {
        setColor(198, 220, 230);
        drawFilledRect(x1 + 2, y1 + 2, x2 - 2, y2 - 2);
    }

    setColor(235, 240, 242);
    drawLine(x1, y1, x2, y1, 1.0f);
    drawLine(x1, y1, x1, y2, 1.0f);

    if (isNight)
        setColor(140, 110, 72);
    else
        setColor(150, 160, 165);

    drawLine((x1 + x2) * 0.5f, y1 + 2, (x1 + x2) * 0.5f, y2 - 2, 1.0f);
    drawLine(x1 + 2, (y1 + y2) * 0.5f, x2 - 2, (y1 + y2) * 0.5f, 1.0f);
}

void drawDoor(float x1, float y1, float x2, float y2,
              unsigned char r, unsigned char g, unsigned char b)
{
    setColor(r, g, b);
    drawFilledRect(x1, y1, x2, y2);

    setColor((unsigned char)(r > 35 ? r - 35 : 0),
             (unsigned char)(g > 35 ? g - 35 : 0),
             (unsigned char)(b > 35 ? b - 35 : 0));
    drawLine(x2, y1, x2, y2, 1.4f);

    setColor(230, 190, 110);
    drawCircle(x2 - 4.0f, (y1 + y2) * 0.5f, 1.8f, 18);
}

void drawApartmentLeft()
{
    setColor(142, 136, 132);
    float groundShadow[][2] = {
        {92, 282}, {278, 282}, {324, 318}, {324, 534}, {132, 534}, {92, 496}};
    drawPolygon(groundShadow, 6);

    setColor(226, 216, 208);
    float front[][2] = {
        {112, 286}, {260, 286}, {298, 318}, {298, 529}, {144, 529}, {112, 497}};
    drawPolygon(front, 6);

    setColor(196, 182, 172);
    float side[][2] = {
        {298, 318}, {318, 334}, {318, 520}, {298, 529}};
    drawPolygon(side, 4);

    setColor(128, 78, 62);
    float roofCap[][2] = {
        {134, 529}, {298, 529}, {318, 520}, {158, 520}};
    drawPolygon(roofCap, 4);

    setColor(114, 68, 54);
    drawFilledRect(144, 520, 298, 526);

    setColor(174, 98, 88);
    drawFilledRect(150, 318, 193, 520);
    setColor(219, 201, 176);
    drawFilledRect(193, 318, 241, 520);
    setColor(162, 168, 174);
    drawFilledRect(241, 318, 298, 520);

    setColor(238, 230, 222);
    for (int y = 344; y <= 490; y += 49)
        drawFilledRect(150, y, 298, y + 3);

    setColor(170, 162, 154);
    for (int y = 336; y <= 483; y += 49)
        drawLine(150, y, 298, y, 0.8f);

    drawWindow(160, 452, 182, 486);
    drawWindow(160, 401, 182, 435);
    drawWindow(160, 350, 182, 384);

    drawWindow(208, 452, 230, 486);
    drawWindow(208, 401, 230, 435);
    drawWindow(208, 350, 230, 384);

    drawWindow(262, 452, 284, 486);
    drawWindow(262, 401, 284, 435);
    drawWindow(262, 350, 284, 384);

    setColor(188, 180, 172);
    drawFilledRect(156, 439, 286, 443);
    drawFilledRect(156, 388, 286, 392);

    setColor(92, 64, 46);
    drawDoor(206, 286, 234, 332, 104, 66, 44);

    setColor(214, 206, 196);
    drawFilledRect(199, 332, 241, 336);

    setColor(162, 150, 142);
    drawFilledRect(108, 286, 302, 292);

    setColor(180, 170, 160);
    float plinth[][2] = {
        {86, 286}, {314, 286}, {298, 318}, {112, 318}};
    drawPolygon(plinth, 4);

    setColor(124, 120, 118);
    drawLine(112, 286, 260, 286, 1.5f);
}

void drawBlueBuilding()
{
    setColor(56, 122, 190);
    drawFilledRect(515, 317, 597, 463);

    setColor(42, 96, 154);
    float side[][2] = {
        {597, 317}, {614, 331}, {614, 476}, {597, 463}};
    drawPolygon(side, 4);

    setColor(88, 154, 218);
    drawFilledRect(519, 444, 593, 458);
    drawFilledRect(519, 404, 593, 418);
    drawFilledRect(519, 364, 593, 378);

    setColor(204, 218, 228);
    drawFilledRect(520, 463, 597, 472);

    setColor(48, 112, 180);
    drawFilledRect(523, 325, 539, 458);
    drawFilledRect(573, 325, 589, 458);

    setColor(70, 136, 202);
    drawFilledRect(542, 325, 570, 458);

    drawWindow(545, 424, 567, 452);
    drawWindow(545, 384, 567, 412);
    drawWindow(545, 344, 567, 372);

    setColor(216, 228, 236);
    drawFilledRect(542, 419, 570, 423);
    drawFilledRect(542, 379, 570, 383);

    setColor(84, 58, 44);
    drawDoor(548, 317, 566, 348, 92, 66, 46);

    setColor(180, 194, 206);
    drawFilledRect(545, 348, 569, 351);

    setColor(160, 178, 194);
    drawLine(515, 463, 597, 463, 2.0f);
    drawLine(597, 317, 597, 463, 1.2f);

    setColor(136, 146, 154);
    drawFilledRect(513, 317, 600, 322);
}

void drawCreamHouseCenter()
{
    setColor(236, 223, 196);
    drawFilledRect(670, 396, 727, 466);

    setColor(210, 194, 168);
    float side[][2] = {
        {727, 396}, {741, 407}, {741, 474}, {727, 466}};
    drawPolygon(side, 4);

    setColor(146, 86, 62);
    drawTriangle(664, 466, 698, 501, 733, 466);

    setColor(116, 66, 50);
    float roofSide[][2] = {
        {733, 466}, {744, 474}, {709, 507}, {698, 501}};
    drawPolygon(roofSide, 4);

    setColor(250, 242, 230);
    drawFilledRect(675, 458, 722, 462);
    drawFilledRect(675, 398, 722, 402);

    drawWindow(679, 426, 692, 451);
    drawWindow(704, 426, 717, 451);

    setColor(212, 198, 176);
    drawFilledRect(677, 420, 719, 424);

    drawDoor(694, 396, 703, 427, 128, 84, 56);

    setColor(188, 176, 154);
    drawFilledRect(690, 427, 707, 430);
    drawFilledRect(668, 396, 730, 400);

    setColor(154, 144, 128);
    drawLine(670, 396, 727, 396, 1.0f);
}

void drawWarehouse()
{
    setColor(162, 160, 188);
    float roof[][2] = {
        {760, 300}, {916, 300}, {928, 409}, {785, 423}, {736, 388}};
    drawPolygon(roof, 5);

    setColor(196, 192, 216);
    float face[][2] = {
        {760, 300}, {878, 300}, {890, 401}, {785, 414}, {736, 388}};
    drawPolygon(face, 5);

    setColor(138, 134, 164);
    float side[][2] = {
        {878, 300}, {916, 300}, {928, 409}, {890, 401}};
    drawPolygon(side, 4);

    setColor(106, 114, 148);
    drawFilledRect(756, 381, 818, 392);
    drawFilledRect(842, 387, 910, 398);

    setColor(220, 224, 232);
    for (int i = 0; i < 4; i++)
    {
        float y = 325 + i * 20;
        drawLine(764, y, 886, y + 10, 0.8f);
    }

    setColor(184, 94, 42);
    drawDoor(826, 304, 886, 374, 172, 88, 38);

    setColor(132, 74, 34);
    for (int x = 834; x <= 874; x += 10)
        drawLine((float)x, 304, (float)x, 374, 1.0f);

    drawWindow(780, 317, 806, 359);
    drawWindow(850, 320, 868, 340);

    setColor(228, 230, 234);
    drawLine(758, 300, 878, 300, 1.8f);
    drawLine(760, 300, 785, 414, 1.1f);

    setColor(150, 148, 156);
    drawFilledRect(752, 300, 892, 305);
}

void drawBigHouseRight()
{
    // moved a bit to the right and upward to avoid overlapping
    // with the other riverside house near the blue building
    const float hx = 34.0f;
    const float hy = 60.0f;

    setColor(202, 170, 158);
    drawFilledRect(1056 + hx, 427 + hy, 1166 + hx, 494 + hy);
    setColor(184, 148, 136);
    drawFilledRect(1042 + hx, 387 + hy, 1178 + hx, 427 + hy);

    setColor(154, 118, 108);
    float side1[][2] = {
        {1178 + hx, 387 + hy}, {1196 + hx, 398 + hy}, {1196 + hx, 438 + hy}, {1178 + hx, 427 + hy}};
    drawPolygon(side1, 4);

    float side2[][2] = {
        {1166 + hx, 427 + hy}, {1184 + hx, 438 + hy}, {1184 + hx, 506 + hy}, {1166 + hx, 494 + hy}};
    drawPolygon(side2, 4);

    setColor(94, 76, 72);
    drawTriangle(1035 + hx, 494 + hy, 1110 + hx, 538 + hy, 1184 + hx, 494 + hy);

    setColor(72, 58, 56);
    float roofSide[][2] = {
        {1184 + hx, 494 + hy}, {1199 + hx, 503 + hy}, {1124 + hx, 547 + hy}, {1110 + hx, 538 + hy}};
    drawPolygon(roofSide, 4);

    setColor(240, 232, 224);
    drawFilledRect(1048 + hx, 425 + hy, 1174 + hx, 430 + hy);

    drawWindow(1061 + hx, 441 + hy, 1085 + hx, 463 + hy);
    drawWindow(1132 + hx, 441 + hy, 1156 + hx, 463 + hy);
    drawWindow(1061 + hx, 397 + hy, 1085 + hx, 419 + hy);
    drawWindow(1132 + hx, 397 + hy, 1156 + hx, 419 + hy);

    setColor(210, 198, 188);
    drawFilledRect(1054 + hx, 434 + hy, 1164 + hx, 438 + hy);
    drawFilledRect(1054 + hx, 390 + hy, 1164 + hx, 394 + hy);

    drawDoor(1101 + hx, 387 + hy, 1119 + hx, 426 + hy, 108, 72, 48);

    setColor(176, 160, 150);
    drawFilledRect(1098 + hx, 426 + hy, 1122 + hx, 430 + hy);

    setColor(154, 142, 136);
    drawFilledRect(1040 + hx, 387 + hy, 1180 + hx, 392 + hy);
    drawLine(1042 + hx, 427 + hy, 1178 + hx, 427 + hy, 1.1f);
}

void drawRightCreamHouse()
{
    // shifted a bit left and upward so it sits farther from the river,
    // while still appearing behind the small blue riverside building
    const float hx = 10.0f;
    const float hy = 18.0f;

    setColor(241, 221, 198);
    drawFilledRect(1160 + hx, 295 + hy, 1237 + hx, 386 + hy);
    setColor(226, 202, 178);
    drawFilledRect(1237 + hx, 295 + hy, 1278 + hx, 378 + hy);

    setColor(200, 178, 158);
    float side[][2] = {
        {1278 + hx, 295 + hy}, {1290 + hx, 304 + hy}, {1290 + hx, 388 + hy}, {1278 + hx, 378 + hy}
    };
    drawPolygon(side, 4);

    setColor(146, 92, 70);
    drawTriangle(1153 + hx, 386 + hy, 1200 + hx, 430 + hy, 1248 + hx, 386 + hy);
    drawTriangle(1216 + hx, 378 + hy, 1252 + hx, 422 + hy, 1286 + hx, 378 + hy);

    setColor(118, 72, 56);
    float roofSide1[][2] = {
        {1248 + hx, 386 + hy}, {1260 + hx, 393 + hy}, {1212 + hx, 437 + hy}, {1200 + hx, 430 + hy}
    };
    drawPolygon(roofSide1, 4);

    float roofSide2[][2] = {
        {1286 + hx, 378 + hy}, {1296 + hx, 384 + hy}, {1262 + hx, 429 + hy}, {1252 + hx, 422 + hy}
    };
    drawPolygon(roofSide2, 4);

    drawWindow(1172 + hx, 318 + hy, 1191 + hx, 353 + hy);
    drawWindow(1201 + hx, 318 + hy, 1219 + hx, 346 + hy);
    drawWindow(1245 + hx, 323 + hy, 1265 + hx, 356 + hy);

    setColor(245, 236, 226);
    drawFilledRect(1164 + hx, 384 + hy, 1276 + hx, 388 + hy);

    setColor(214, 198, 180);
    drawFilledRect(1168 + hx, 308 + hy, 1228 + hx, 312 + hy);
    drawFilledRect(1240 + hx, 312 + hy, 1272 + hx, 316 + hy);

    drawDoor(1192 + hx, 295 + hy, 1206 + hx, 330 + hy, 124, 84, 54);

    setColor(182, 166, 152);
    drawFilledRect(1188 + hx, 330 + hy, 1210 + hx, 334 + hy);
    drawFilledRect(1158 + hx, 295 + hy, 1278 + hx, 300 + hy);
}

void drawVillageHouse(float x, float y, float s,
                      unsigned char wallR, unsigned char wallG, unsigned char wallB,
                      unsigned char roofR, unsigned char roofG, unsigned char roofB)
{
    setColor(wallR, wallG, wallB);
    drawFilledRect(x, y, x + 52 * s, y + 34 * s);

    setColor((unsigned char)(wallR > 24 ? wallR - 24 : 0),
             (unsigned char)(wallG > 24 ? wallG - 24 : 0),
             (unsigned char)(wallB > 24 ? wallB - 24 : 0));
    float side[][2] = {
        {x + 52 * s, y}, {x + 62 * s, y + 7 * s},
        {x + 62 * s, y + 40 * s}, {x + 52 * s, y + 34 * s}};
    drawPolygon(side, 4);

    setColor(roofR, roofG, roofB);
    drawTriangle(x - 3 * s, y + 34 * s, x + 26 * s, y + 57 * s, x + 55 * s, y + 34 * s);

    setColor((unsigned char)(roofR > 30 ? roofR - 30 : 0),
             (unsigned char)(roofG > 30 ? roofG - 30 : 0),
             (unsigned char)(roofB > 30 ? roofB - 30 : 0));
    float roofSide[][2] = {
        {x + 55 * s, y + 34 * s}, {x + 65 * s, y + 40 * s},
        {x + 36 * s, y + 62 * s}, {x + 26 * s, y + 57 * s}};
    drawPolygon(roofSide, 4);

    setColor((unsigned char)(wallR < 235 ? wallR + 16 : 255),
             (unsigned char)(wallG < 235 ? wallG + 16 : 255),
             (unsigned char)(wallB < 235 ? wallB + 16 : 255));
    drawFilledRect(x + 3 * s, y + 28 * s, x + 49 * s, y + 31 * s);

    drawWindow(x + 7 * s, y + 15 * s, x + 18 * s, y + 27 * s);
    drawWindow(x + 34 * s, y + 15 * s, x + 45 * s, y + 27 * s);
    drawDoor(x + 22 * s, y, x + 31 * s, y + 21 * s, 116, 78, 50);

    setColor(178, 164, 148);
    drawFilledRect(x - 2 * s, y - 3 * s, x + 54 * s, y);

    setColor(154, 142, 128);
    drawLine(x, y, x + 52 * s, y, 0.9f);
}

void drawBigRoundTree(float x, float y, float s)
{
    setColor(108, 72, 38);
    drawFilledRect(x - 7 * s, y, x + 7 * s, y + 34 * s);

    glPushMatrix();
    glTranslatef(x, y + 34 * s, 0.0f);
    glRotatef(getTreeSwayDeg(x, y) * 0.75f, 0.0f, 0.0f, 1.0f);

    setColor(62, 138, 54);
    drawCircle(0, 22 * s, 22 * s);
    drawCircle(-18 * s, 16 * s, 17 * s);
    drawCircle(18 * s, 16 * s, 17 * s);
    drawCircle(-7 * s, 36 * s, 14 * s);
    drawCircle(8 * s, 37 * s, 14 * s);

    setColor(84, 170, 70);
    drawCircle(-5 * s, 20 * s, 10 * s);
    drawCircle(11 * s, 14 * s, 9 * s);

    glPopMatrix();
}

void drawTinyHousesAndFarObjects()
{
    // extra houses on empty land only, away from road and river
    drawVillageHouse(54, 528, 1.05f, 232, 214, 188, 142, 88, 68);
    drawVillageHouse(332, 548, 0.95f, 214, 198, 176, 120, 76, 58);
    drawVillageHouse(780, 510, 0.88f, 236, 220, 194, 138, 82, 60);
}

void drawCloud(float x, float y, float s)
{
    setColor(246, 208, 244);
    drawCircle(x, y, 24 * s);
    drawCircle(x + 22 * s, y + 6 * s, 20 * s);
    drawCircle(x + 47 * s, y, 25 * s);
    drawCircle(x + 24 * s, y - 8 * s, 23 * s);
}

void drawFarHills()
{
    // far atmospheric mountain layer
    setColor(118, 164, 178);
    float farBack[][2] = {
        {0, 640}, {0, 780}, {170, 780}, {260, 748}, {380, 760},
        {510, 716}, {635, 738}, {770, 698}, {910, 726}, {1055, 686},
        {1205, 718}, {1322, 690}, {1440, 706}, {1440, 640}};
    drawPolygon(farBack, 14);

    // left mountain mass
    setColor(74, 132, 108);
    float leftHill[][2] = {
        {0, 620}, {0, 710}, {85, 698}, {170, 722}, {258, 694},
        {350, 706}, {438, 674}, {540, 650}, {620, 620}};
    drawPolygon(leftHill, 9);

    // center mountain range
    setColor(58, 146, 118);
    float midHill[][2] = {
        {430, 592}, {520, 636}, {618, 688}, {720, 650}, {806, 682},
        {910, 708}, {1026, 676}, {1134, 626}, {1186, 592}};
    drawPolygon(midHill, 9);

    // right mountain range
    setColor(67, 136, 112);
    float rightHill[][2] = {
        {1010, 582}, {1100, 620}, {1182, 676}, {1265, 646},
        {1356, 690}, {1440, 670}, {1440, 572}, {1200, 560}};
    drawPolygon(rightHill, 8);

    // warm highlight ridges
    setColor(116, 176, 136);
    float leftHighlight[][2] = {
        {86, 676}, {162, 700}, {246, 674}, {310, 680}, {246, 646}, {146, 650}};
    drawPolygon(leftHighlight, 6);

    float midHighlight[][2] = {
        {610, 660}, {692, 634}, {774, 650}, {858, 680}, {948, 662}, {876, 628}, {718, 616}};
    drawPolygon(midHighlight, 7);

    float rightHighlight[][2] = {
        {1160, 654}, {1232, 630}, {1298, 644}, {1360, 674}, {1326, 620}, {1228, 606}};
    drawPolygon(rightHighlight, 6);

    // mist band between mountain and land
    setColor(178, 218, 196);
    drawEllipse(248, 612, 228, 24, 90);
    drawEllipse(768, 590, 290, 28, 100);
    drawEllipse(1216, 580, 238, 24, 90);

    // foreground land shape
    setColor(201, 232, 176);
    float land[][2] = {
        {0, 260}, {0, 620}, {126, 614}, {242, 604}, {360, 612}, {482, 600},
        {612, 580}, {734, 560}, {878, 548}, {1020, 548}, {1164, 552},
        {1304, 558}, {1440, 555}, {1440, 260}};
    drawPolygon(land, 14);

    // gentle shadow sweep on foothills
    setColor(164, 208, 150);
    float foothillShadow[][2] = {
        {0, 560}, {128, 572}, {248, 568}, {372, 578}, {486, 566}, {612, 548},
        {756, 540}, {922, 540}, {1080, 546}, {1268, 552}, {1440, 548}, {1440, 522},
        {1200, 516}, {1008, 514}, {820, 516}, {628, 522}, {420, 534}, {220, 540}, {0, 536}};
    drawPolygon(foothillShadow, 19);
}


void drawShrub(float x, float y, float s)
{
    setColor(78, 150, 64);
    drawCircle(x, y, 9 * s, 24);
    drawCircle(x - 8 * s, y - 1 * s, 7 * s, 22);
    drawCircle(x + 8 * s, y - 1 * s, 7 * s, 22);

    setColor(102, 178, 82);
    drawCircle(x - 3 * s, y + 2 * s, 4 * s, 18);
    drawCircle(x + 4 * s, y + 1 * s, 3.5f * s, 18);
}

void drawTuft(float x, float y, float s)
{
    setColor(86, 156, 62);
    drawLine(x, y, x - 4 * s, y + 13 * s, 1.0f);
    drawLine(x, y, x, y + 16 * s, 1.0f);
    drawLine(x, y, x + 4 * s, y + 12 * s, 1.0f);
    drawLine(x, y, x - 2 * s, y + 10 * s, 1.0f);
    drawLine(x, y, x + 2 * s, y + 10 * s, 1.0f);
}

void drawLandDetails()
{
    // gentle color variation on the ground (kept away from the river bank)
    setColor(188, 221, 152);
    drawEllipse(170, 208, 150, 34, 60);
    drawEllipse(430, 182, 190, 42, 70);
    drawEllipse(700, 194, 135, 34, 60);

    setColor(173, 208, 140);
    drawEllipse(92, 144, 94, 24, 54);
    drawEllipse(308, 122, 110, 26, 56);
    drawEllipse(614, 140, 126, 28, 58);
    drawEllipse(736, 126, 62, 18, 42);

    // dry soil / worn patches
    setColor(198, 188, 136);
    drawEllipse(214, 84, 54, 16, 42);
    drawEllipse(512, 86, 62, 18, 46);
    drawEllipse(718, 78, 34, 11, 34);

    setColor(182, 171, 120);
    drawEllipse(250, 80, 24, 7, 24);
    drawEllipse(546, 82, 27, 8, 24);
    drawEllipse(742, 74, 16, 5, 18);

    // low shrubs across the land
    drawShrub(88, 256, 1.1f);
    drawShrub(132, 246, 0.9f);
    drawShrub(244, 228, 1.0f);
    drawShrub(348, 252, 0.95f);
    drawShrub(468, 236, 1.1f);
    drawShrub(590, 228, 0.92f);
    drawShrub(690, 244, 0.92f);
    drawShrub(758, 236, 0.82f);
}

void drawWildFlowers()
{
    float pts[][3] = {
        {118, 114, 0.9f}, {168, 126, 0.8f}, {286, 116, 0.85f}, {384, 100, 0.8f},
        {542, 120, 0.9f}, {714, 146, 0.85f}, {646, 108, 0.78f}, {760, 118, 0.72f}
    };

    for (int i = 0; i < 8; i++)
    {
        float x = pts[i][0];
        float y = pts[i][1];
        float s = pts[i][2];

        setColor(78, 146, 64);
        drawLine(x, y, x, y + 10 * s, 1.0f);

        setColor(238, 212, 78);
        drawCircle(x, y + 12 * s, 2.0f * s, 14);

        setColor(236, 236, 244);
        drawCircle(x - 3 * s, y + 12 * s, 1.8f * s, 12);
        drawCircle(x + 3 * s, y + 12 * s, 1.8f * s, 12);
        drawCircle(x, y + 15 * s, 1.8f * s, 12);
        drawCircle(x, y + 9 * s, 1.8f * s, 12);
    }
}

void drawGroundTexture()
{
    // hand-placed grass tufts kept clear of the river shape
    drawTuft(58, 102, 0.9f);   drawTuft(76, 110, 0.8f);
    drawTuft(146, 96, 0.95f);  drawTuft(196, 122, 0.9f);
    drawTuft(282, 102, 0.8f);  drawTuft(362, 88, 0.85f);
    drawTuft(414, 114, 0.9f);  drawTuft(488, 104, 0.95f);
    drawTuft(612, 92, 0.9f);   drawTuft(676, 124, 1.0f);
    drawTuft(724, 98, 0.78f);  drawTuft(770, 112, 0.76f);

    // subtle darker meadow shadows
    setColor(154, 196, 126);
    drawEllipse(176, 58, 88, 11, 44);
    drawEllipse(494, 54, 104, 12, 48);
    drawEllipse(724, 58, 68, 9, 34);
}

void drawGrassPatches()
{
    setColor(177, 220, 135);
    drawEllipse(110, 90, 55, 18);
    drawEllipse(230, 115, 60, 20);
    drawEllipse(330, 75, 48, 16);
    drawEllipse(560, 95, 62, 19);
    drawEllipse(700, 130, 70, 22);
    drawEllipse(640, 82, 44, 15);
    drawEllipse(736, 88, 42, 14);
}

void drawRiver()
{
    // outer shallow river spread
    setColor(132, 212, 240);
    float outerRiver[][2] = {
        {818, 560}, {896, 556}, {956, 522}, {1018, 458}, {1060, 380},
        {1052, 306}, {1088, 246}, {1148, 214}, {1224, 206}, {1296, 186},
        {1364, 150}, {1418, 96}, {1440, 74}, {1440, 0}, {1198, 0},
        {1164, 24}, {1132, 64}, {1100, 102}, {1048, 128}, {1002, 168},
        {970, 228}, {944, 304}, {936, 384}, {900, 464}, {844, 524}
    };
    drawPolygon(outerRiver, 25);

    // main river body
    setColor(58, 178, 230);
    float riverMain[][2] = {
        {850, 550}, {910, 548}, {972, 504}, {1016, 438}, {1036, 366},
        {1026, 298}, {1058, 248}, {1120, 214}, {1192, 206}, {1264, 186},
        {1334, 148}, {1390, 92}, {1440, 44}, {1440, 0}, {1224, 0},
        {1186, 28}, {1154, 68}, {1118, 108}, {1064, 138}, {1022, 186},
        {988, 246}, {972, 316}, {972, 392}, {944, 462}, {890, 524}
    };
    drawPolygon(riverMain, 25);

    // deeper channel
    setColor(32, 144, 206);
    float deepFlow[][2] = {
        {906, 538}, {958, 496}, {1000, 436}, {1018, 364}, {1014, 304},
        {1040, 260}, {1090, 232}, {1160, 218}, {1234, 194}, {1298, 156},
        {1354, 102}, {1408, 40}, {1440, 12}, {1440, 0}, {1260, 0},
        {1216, 36}, {1180, 76}, {1140, 112}, {1088, 146}, {1048, 194},
        {1020, 252}, {1008, 314}, {1008, 378}, {986, 444}, {948, 498}
    };
    drawPolygon(deepFlow, 25);

    // dark narrow center strip for depth
    setColor(18, 116, 182);
    float centerFlow[][2] = {
        {950, 500}, {988, 444}, {1006, 378}, {1006, 316}, {1018, 266},
        {1054, 232}, {1114, 214}, {1190, 196}, {1258, 164}, {1314, 118},
        {1368, 62}, {1418, 10}, {1440, 0}, {1286, 0}, {1242, 34},
        {1204, 74}, {1158, 110}, {1102, 142}, {1066, 186}, {1038, 240},
        {1018, 298}, {1004, 364}, {982, 430}
    };
    drawPolygon(centerFlow, 23);

    // water extension under flyover
    setColor(80, 190, 236);
    float underFlyover[][2] = {
        {772, 258}, {850, 266}, {930, 258}, {1008, 238}, {1084, 218},
        {1154, 206}, {1210, 198}, {1242, 182}, {1218, 156}, {1166, 150},
        {1100, 160}, {1038, 176}, {962, 196}, {888, 214}, {820, 224}, {772, 226}
    };
    drawPolygon(underFlyover, 16);

    // lower spread
    setColor(60, 170, 224);
    float lowerSpread[][2] = {
        {1094, 162}, {1168, 150}, {1242, 140}, {1314, 118},
        {1388, 72}, {1440, 18}, {1440, 0}, {1208, 0},
        {1170, 24}, {1132, 66}, {1098, 104}
    };
    drawPolygon(lowerSpread, 11);

    // wet shoreline and sandy banks
    setColor(214, 231, 184);
    float shore1[][2] = {
        {836, 544}, {888, 540}, {924, 516}, {894, 510}, {850, 524}
    };
    drawPolygon(shore1, 5);

    float shore2[][2] = {
        {1182, 202}, {1240, 188}, {1300, 162}, {1272, 154}, {1208, 170}
    };
    drawPolygon(shore2, 5);

    float shore3[][2] = {
        {1106, 146}, {1160, 136}, {1212, 128}, {1180, 120}, {1130, 126}
    };
    drawPolygon(shore3, 5);

    // darker wet bank accents
    setColor(176, 205, 150);
    drawEllipse(874, 522, 34, 8, 36);
    drawEllipse(1218, 170, 42, 9, 38);
    drawEllipse(1154, 126, 36, 8, 34);

    // preserve land silhouette on left/bottom
    setColor(201, 232, 176);
    float landPatch[][2] = {
        {0, 0},
        {1208, 0},
        {1168, 24},
        {1134, 64},
        {1102, 102},
        {1048, 128},
        {1002, 168},
        {970, 228},
        {878, 260},
        {0, 260}
    };
    drawPolygon(landPatch, 10);

    // fixed soft reflections
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4ub(190, 236, 248, 64);
    drawEllipse(924, 504, 40, 8, 52);
    drawEllipse(998, 402, 28, 6, 48);
    drawEllipse(1036, 284, 34, 7, 48);
    drawEllipse(1186, 186, 42, 7, 48);
    drawEllipse(1328, 108, 48, 8, 48);

    glColor4ub(232, 248, 252, 44);
    drawEllipse(966, 454, 20, 4, 36);
    drawEllipse(1018, 338, 22, 4, 36);
    drawEllipse(1108, 220, 28, 4, 36);
    drawEllipse(1268, 154, 24, 4, 34);

    // thin bank foam
    glColor4ub(240, 250, 255, 42);
    drawEllipse(852, 528, 22, 3, 32);
    drawEllipse(896, 512, 18, 3, 30);
    drawEllipse(1212, 174, 26, 3, 34);
    drawEllipse(1150, 128, 20, 3, 30);

    glDisable(GL_BLEND);
}


void drawRiverAnimation()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // drifting long surface reflections that follow the river path
    for (int i = 0; i < 11; i++)
    {
        float tt = riverAnim + i * 0.09f;
        while (tt > 1.0f) tt -= 1.0f;

        Vec2 p = getBoatPosition(tt);

        float rx = lerp(16.0f, 46.0f, 1.0f - tt);
        float ry = lerp(2.4f, 7.2f, 1.0f - tt);

        glColor4ub(210, 244, 255, 44);
        drawEllipse(p.x - 12.0f, p.y + 5.0f, rx, ry, 42);

        glColor4ub(244, 252, 255, 66);
        drawEllipse(p.x + 12.0f, p.y - 1.0f, rx * 0.45f, ry * 0.45f, 34);
    }

    // moving cross-ripples in the broader lower river
    for (int i = 0; i < 8; i++)
    {
        float phase = riverAnim * 2.0f * PI + i * 0.72f;
        float x = 1088.0f + i * 42.0f + sin(phase) * 12.0f;
        float y = 86.0f + i * 18.0f + cos(phase * 0.7f) * 4.0f;

        glColor4ub(212, 242, 255, 36);
        drawEllipse(x, y, 26.0f, 3.6f, 34);

        glColor4ub(242, 252, 255, 54);
        drawEllipse(x + 8.0f, y + 1.0f, 11.0f, 1.5f, 28);
    }

    // subtle current streaks under the flyover
    for (int i = 0; i < 6; i++)
    {
        float phase = riverAnim * 2.0f * PI + i * 0.9f;
        float x = 822.0f + i * 68.0f + sin(phase) * 10.0f;
        float y = 222.0f + cos(phase * 0.8f) * 3.0f;

        glColor4ub(220, 246, 255, 30);
        drawEllipse(x, y, 24.0f, 3.0f, 30);
    }

    glDisable(GL_BLEND);
}


void drawStreetLight(float x, float y, float h)
{
    setColor(88, 88, 92);
    drawFilledRect(x - 2, y, x + 2, y + h);

    setColor(88, 88, 92);
    drawLine(x, y + h, x + 16, y + h + 6, 2.0f);

    if (isNight)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glColor4ub(255, 238, 170, 55);
        drawCircle(x + 18, y + h + 5, 14, 30);
        glColor4ub(255, 238, 170, 90);
        drawCircle(x + 18, y + h + 5, 9, 26);

        glDisable(GL_BLEND);
    }

    setColor(245, 225, 150);
    drawFilledRect(x + 14, y + h + 1, x + 22, y + h + 8);
}

void drawGuardRailPost(float x, float y, float h)
{
    setColor(150, 150, 155);
    drawFilledRect(x - 2, y - h, x + 2, y);
}

void drawCurveGuardRail()
{
    setColor(185, 185, 190);
    drawBezierOffsetLine(38.0f, 3.0f, 64);
    drawBezierOffsetLine(44.0f, 2.0f, 64);

    for (int i = 0; i <= 8; i++)
    {
        float t = i / 8.0f;
        Vec2 p = bezierPoint(t);
        Vec2 tan = normalizeVec(bezierTangent(t));
        Vec2 n = makeVec(-tan.y, tan.x);
        Vec2 q = addVec(p, mulVec(n, 38.0f));
        drawGuardRailPost(q.x, q.y, 12.0f);
    }

    setColor(185, 185, 190);
    drawLine(HROAD_STRAIGHT_START_X, HROAD_CENTER_Y + TURN_START_WIDTH * 0.5f + 6.0f,
             HROAD_END_X - 10.0f, HROAD_CENTER_Y + TURN_END_WIDTH * 0.5f + 7.0f, 3.0f);
    drawLine(HROAD_STRAIGHT_START_X, HROAD_CENTER_Y + TURN_START_WIDTH * 0.5f + 12.0f,
             HROAD_END_X - 10.0f, HROAD_CENTER_Y + TURN_END_WIDTH * 0.5f + 13.0f, 2.0f);

    for (int i = 0; i < 8; i++)
    {
        float t = i / 7.0f;
        float px = lerp(HROAD_STRAIGHT_START_X + 15.0f, HROAD_END_X - 20.0f, t);
        float py = lerp(HROAD_CENTER_Y + TURN_START_WIDTH * 0.5f + 6.0f,
                        HROAD_CENTER_Y + TURN_END_WIDTH * 0.5f + 7.0f, t);
        drawGuardRailPost(px, py, 12.0f);
    }
}

void drawRoadShadow()
{
    setColor(95, 98, 102);
    drawBezierRibbonShadow(4.0f, -5.0f, 8.0f, 80);

    setColor(102, 104, 108);
    float shadow[][2] = {
        {HROAD_STRAIGHT_START_X, HROAD_CENTER_Y - TURN_START_WIDTH * 0.5f - 6.0f},
        {HROAD_END_X,            HROAD_CENTER_Y - TURN_END_WIDTH * 0.5f - 4.0f},
        {HROAD_END_X,            HROAD_CENTER_Y - TURN_END_WIDTH * 0.5f + 6.0f},
        {HROAD_STRAIGHT_START_X, HROAD_CENTER_Y - TURN_START_WIDTH * 0.5f + 8.0f}
    };
    drawPolygon(shadow, 4);
}

void drawCurveEdgeHighlight()
{
    setColor(165, 165, 170);
    drawBezierOffsetLine(0.0f, 1.2f, 80);

    setColor(165, 165, 170);
    drawLine(HROAD_STRAIGHT_START_X, HROAD_CENTER_Y + TURN_START_WIDTH * 0.5f - 3.0f,
             HROAD_END_X, HROAD_CENTER_Y + TURN_END_WIDTH * 0.5f - 3.0f, 1.2f);
}

// new: side white lines for more realistic road edge
void drawRoadSideLines()
{
    // curve side lines
    setColor(242, 242, 242);
    drawBezierEdge(true, 1.8f, 90);
    drawBezierEdge(false, 1.8f, 90);

    // horizontal side lines
    drawLine(HROAD_STRAIGHT_START_X, HROAD_CENTER_Y - TURN_START_WIDTH * 0.5f + SIDE_LINE_MARGIN,
             HROAD_END_X,            HROAD_CENTER_Y - TURN_END_WIDTH * 0.5f + SIDE_LINE_MARGIN, 1.8f);
    drawLine(HROAD_STRAIGHT_START_X, HROAD_CENTER_Y + TURN_START_WIDTH * 0.5f - SIDE_LINE_MARGIN,
             HROAD_END_X,            HROAD_CENTER_Y + TURN_END_WIDTH * 0.5f - SIDE_LINE_MARGIN, 1.8f);
}

// new: subtle shoulder band feel
void drawRoadShoulderFeel()
{
    setColor(148, 150, 154);

    // left and right small highlight on curve
    drawBezierOffsetLine(2.0f, 0.8f, 80);
    drawBezierOffsetLine(-2.0f, 0.8f, 80);

    // horizontal shoulder hints
    drawLine(HROAD_STRAIGHT_START_X, HROAD_CENTER_Y - TURN_START_WIDTH * 0.5f + 7.0f,
             HROAD_END_X,            HROAD_CENTER_Y - TURN_END_WIDTH * 0.5f + 5.0f, 0.8f);
    drawLine(HROAD_STRAIGHT_START_X, HROAD_CENTER_Y + TURN_START_WIDTH * 0.5f - 7.0f,
             HROAD_END_X,            HROAD_CENTER_Y + TURN_END_WIDTH * 0.5f - 5.0f, 0.8f);
}

void drawTurnedRoadMarkings()
{
    // vertical perspective dashes
    for (int i = 0; i < 8; i++)
    {
        float t = i / 7.0f;
        float y = lerp(30.0f, VROAD_STRAIGHT_END_Y - 30.0f, t);

        float cx = perspectiveRoadCenterX(y);
        float dashLen = lerp(42.0f, 12.0f, y / 600.0f);
        float dashW = lerp(6.0f, 2.0f, y / 600.0f);

        setColor(230, 230, 230);
        drawFilledRect(cx - dashW / 2.0f, y, cx + dashW / 2.0f, y + dashLen);
    }

    // smooth curve dashes
    for (int i = 0; i < 6; i++)
    {
        float t = 0.10f + i * 0.14f;

        Vec2 p = bezierPoint(t);
        Vec2 tan = normalizeVec(bezierTangent(t));
        float angDeg = atan2(tan.y, tan.x) * 180.0f / PI - 90.0f;

        glPushMatrix();
        glTranslatef(p.x, p.y, 0.0f);
        glRotatef(angDeg, 0.0f, 0.0f, 1.0f);

        float dashLen = 14.0f;
        float dashW = 2.4f;
        setColor(230, 230, 230);
        drawFilledRect(-dashW / 2.0f, -dashLen / 2.0f, dashW / 2.0f, dashLen / 2.0f);

        glPopMatrix();
    }

    // horizontal dashes
    for (int i = 0; i < 8; i++)
    {
        float tx = i / 7.0f;
        float x = lerp(HROAD_STRAIGHT_START_X + 24.0f, HROAD_END_X - 55.0f, tx);
        float dashH = lerp(3.2f, 1.8f, tx);

        setColor(230, 230, 230);
        drawFilledRect(x, HROAD_CENTER_Y - dashH / 2.0f,
                       x + lerp(24.0f, 12.0f, tx), HROAD_CENTER_Y + dashH / 2.0f);
    }
}

void drawRoads()
{
    float yBottom = 0.0f, yTop = VROAD_STRAIGHT_END_Y;
    float cBottom = perspectiveRoadCenterX(yBottom);
    float cTop = perspectiveRoadCenterX(yTop);
    float wBottom = perspectiveRoadWidth(yBottom);
    float wTop = perspectiveRoadWidth(yTop);

    drawRoadShadow();

    // main vertical road
    setColor(136, 138, 143);
    float road1[][2] = {
        {cBottom - wBottom / 2.0f, yBottom},
        {cBottom + wBottom / 2.0f, yBottom},
        {cTop + wTop / 2.0f, yTop},
        {cTop - wTop / 2.0f, yTop}};
    drawPolygon(road1, 4);

    setColor(110, 110, 115);
    drawLine(cBottom - wBottom / 2.0f, yBottom, cTop - wTop / 2.0f, yTop, 2.0f);
    drawLine(cBottom + wBottom / 2.0f, yBottom, cTop + wTop / 2.0f, yTop, 2.0f);

    // smooth curve road
    setColor(136, 138, 143);
    drawBezierRoadBand(90);

    setColor(110, 110, 115);
    drawBezierEdge(true, 2.0f, 90);
    drawBezierEdge(false, 2.0f, 90);

    // horizontal road
    setColor(136, 138, 143);
    float road2[][2] = {
        {HROAD_STRAIGHT_START_X, HROAD_CENTER_Y - TURN_START_WIDTH / 2.0f},
        {HROAD_END_X,            HROAD_CENTER_Y - TURN_END_WIDTH / 2.0f},
        {HROAD_END_X,            HROAD_CENTER_Y + TURN_END_WIDTH / 2.0f},
        {HROAD_STRAIGHT_START_X, HROAD_CENTER_Y + TURN_START_WIDTH / 2.0f}};
    drawPolygon(road2, 4);

    setColor(110, 110, 115);
    drawLine(HROAD_STRAIGHT_START_X, HROAD_CENTER_Y - TURN_START_WIDTH / 2.0f,
             HROAD_END_X,            HROAD_CENTER_Y - TURN_END_WIDTH / 2.0f, 2.0f);
    drawLine(HROAD_STRAIGHT_START_X, HROAD_CENTER_Y + TURN_START_WIDTH / 2.0f,
             HROAD_END_X,            HROAD_CENTER_Y + TURN_END_WIDTH / 2.0f, 2.0f);

    drawRoadShoulderFeel();
    drawCurveEdgeHighlight();
    drawRoadSideLines();
    drawTurnedRoadMarkings();
    drawCurveGuardRail();

    // left top small road
    setColor(136, 138, 143);
    float road3[][2] = {
        {455, 545}, {560, 550}, {655, 560}, {710, 574},
        {690, 590}, {600, 575}, {455, 565}};
    drawPolygon(road3, 7);

    // right city road
    float road4[][2] = {
        {1040, 425}, {1180, 425}, {1180, 450}, {1040, 450}};
    drawPolygon(road4, 4);

    // street lights
    drawStreetLight(355, 455, 65);
    drawStreetLight(548, 585, 55);
    drawStreetLight(740, 578, 60);
    drawStreetLight(915, 574, 58);
}


void drawFlyoverLamp(float x, float baseY, float h, bool upperSide)
{
    setColor(92, 92, 96);
    drawFilledRect(x - 2, baseY, x + 2, baseY + h);

    setColor(92, 92, 96);
    if (upperSide)
    {
        drawLine(x, baseY + h, x + 18, baseY + h + 8, 2.0f);

        if (isNight)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glColor4ub(255, 240, 175, 55);
            drawCircle(x + 18, baseY + h + 8, 14, 30);
            glColor4ub(255, 240, 175, 90);
            drawCircle(x + 18, baseY + h + 8, 9, 26);

            glDisable(GL_BLEND);
        }

        setColor(248, 232, 164);
        drawFilledRect(x + 14, baseY + h + 3, x + 23, baseY + h + 10);
    }
    else
    {
        drawLine(x, baseY + h, x - 18, baseY + h + 8, 2.0f);

        if (isNight)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glColor4ub(255, 240, 175, 55);
            drawCircle(x - 18, baseY + h + 8, 14, 30);
            glColor4ub(255, 240, 175, 90);
            drawCircle(x - 18, baseY + h + 8, 9, 26);

            glDisable(GL_BLEND);
        }

        setColor(248, 232, 164);
        drawFilledRect(x - 23, baseY + h + 3, x - 14, baseY + h + 10);
    }
}

void drawFlyoverLamps()
{
    const float upperBaseY = 221.0f;
    const float lowerBaseY = 184.0f;

    // upper edge lamps
    drawFlyoverLamp(110.0f, upperBaseY, 40.0f, true);
    drawFlyoverLamp(300.0f, upperBaseY, 40.0f, true);
    drawFlyoverLamp(520.0f, upperBaseY, 40.0f, true);
    drawFlyoverLamp(760.0f, upperBaseY, 40.0f, true);
    drawFlyoverLamp(1010.0f, upperBaseY, 40.0f, true);
    drawFlyoverLamp(1260.0f, upperBaseY, 40.0f, true);

    // lower edge lamps
    drawFlyoverLamp(170.0f, lowerBaseY, 40.0f, false);
    drawFlyoverLamp(410.0f, lowerBaseY, 40.0f, false);
    drawFlyoverLamp(650.0f, lowerBaseY, 40.0f, false);
    drawFlyoverLamp(900.0f, lowerBaseY, 40.0f, false);
    drawFlyoverLamp(1150.0f, lowerBaseY, 40.0f, false);
    drawFlyoverLamp(1360.0f, lowerBaseY, 40.0f, false);
}

void drawHighway()
{
    setColor(123, 124, 129);
    drawFilledRect(0, 170, 1440, 235);

    setColor(180, 180, 185);
    drawFilledRect(0, 151, 1440, 170);

    setColor(95, 95, 100);
    drawFilledRect(0, 229, 1440, 235);
    drawFilledRect(0, 170, 1440, 176);

    setColor(70, 70, 74);
    drawFilledRect(0, 136, 1440, 170);

    // flyover center divider for two-way traffic
    setColor(112, 172, 58);
    drawFilledRect(0, 200, 1440, 205);

    setColor(235, 235, 235);
    for (int x = 28; x < 1440; x += 96)
    {
        drawFilledRect((float)x, 182, (float)x + 34, 185);
        drawFilledRect((float)x, 218, (float)x + 34, 221);
    }

    setColor(250, 228, 90);
    for (int x = 80; x < 1440; x += 120)
        drawFilledRect((float)x, 201, (float)x + 46, 204);

    setColor(160, 160, 165);
    drawFilledRect(470, 32, 490, 151);
    drawFilledRect(880, 32, 900, 151);
    drawFilledRect(1400, 32, 1418, 151);

    setColor(135, 135, 140);
    drawFilledRect(466, 151, 494, 158);
    drawFilledRect(876, 151, 904, 158);
    drawFilledRect(1396, 151, 1422, 158);

    drawFlyoverLamps();
}

void drawWheel(float x, float y, float r)
{
    setColor(35, 35, 35);
    drawCircle(x, y, r);

    setColor(170, 170, 170);
    drawCircle(x, y, r * 0.45f);
}


/*
Algorithm: Parametric path animation
Use: Boat position is computed from a time parameter t. Linear interpolation
     gives the main path, while sine terms add gentle drift.
*/
Vec2 getBoatPosition(float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    // keep the boat on the river center flow and below the flyover / rail layers
    float x = lerp(1298.0f, 968.0f, t);
    float y = lerp(18.0f, 520.0f, t);

    // gentle drift only, so the boat moves mostly straight forward
    x += sin(t * PI * 1.8f) * 10.0f;
    y += sin(t * PI * 3.2f) * 4.0f;

    return makeVec(x, y);
}

/*
Technique: Depth-based scaling / perspective
Use: Boat size changes according to river position so distant boats appear smaller.
*/
float getBoatScale(float t)
{
    Vec2 p = getBoatPosition(t);

    // perspective: near bottom looks bigger, far upper river looks smaller
    float yRatio = clamp01((p.y - 18.0f) / (520.0f - 18.0f));
    return lerp(0.72f, 0.38f, yRatio);
}

/*
Algorithm: State-based orientation control with eased turning
Use: Chooses boat angle depending on forward/reverse state and smoothstep easing.
*/
float getBoatAngle(float t)
{
    // The boat should look like it is seen from the back while going upward,
    // and instead of making a U-turn at the end it should reverse back downward.
    // So the visible orientation stays mostly the same through the whole trip.
    if (boatForward)
        return 6.0f * smoothstep01((t - 0.88f) / 0.12f);

    if (t < 0.12f)
        return 6.0f * smoothstep01(t / 0.12f);

    return 6.0f;
}

void drawBoat(float x, float y, float s, float angleDeg)
{
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glRotatef(angleDeg, 0.0f, 0.0f, 1.0f);

    // Wake behind the stern: stronger when the boat is moving upward,
    // and still visible when it reverses backward.
    setColor(198, 236, 248);
    drawEllipse(0 * s, -42 * s, 22 * s, 5.2f * s, 42);
    drawEllipse(0 * s, -56 * s, 16 * s, 3.4f * s, 40);
    setColor(225, 247, 252);
    drawEllipse(0 * s, -42 * s, 13 * s, 2.3f * s, 36);
    drawEllipse(0 * s, -56 * s, 9 * s, 1.6f * s, 32);

    // Rear-view Bangladeshi dingi:
    // wider stern near viewer, narrower front away from viewer.
    setColor(92, 57, 30);
    float hull[][2] = {
        {-20 * s, -30 * s}, {-15 * s, -46 * s}, {-8 * s, -58 * s},
        {0 * s, -64 * s}, {8 * s, -58 * s}, {15 * s, -46 * s},
        {20 * s, -30 * s}, {24 * s, 6 * s}, {20 * s, 34 * s},
        {12 * s, 58 * s}, {0 * s, 76 * s}, {-12 * s, 58 * s},
        {-20 * s, 34 * s}, {-24 * s, 6 * s}
    };
    drawPolygon(hull, 14);

    // Darker right-side shade for depth
    setColor(66, 39, 20);
    float hullShade[][2] = {
        {0 * s, -64 * s}, {8 * s, -58 * s}, {15 * s, -46 * s},
        {20 * s, -30 * s}, {24 * s, 6 * s}, {20 * s, 34 * s},
        {12 * s, 58 * s}, {0 * s, 76 * s}, {0 * s, 40 * s},
        {7 * s, 14 * s}, {9 * s, -20 * s}, {7 * s, -46 * s}
    };
    drawPolygon(hullShade, 12);

    // Open deck
    setColor(178, 118, 60);
    float deck[][2] = {
        {-13 * s, -24 * s}, {-9 * s, -36 * s}, {-4 * s, -42 * s},
        {4 * s, -42 * s}, {9 * s, -36 * s}, {13 * s, -24 * s},
        {15 * s, 4 * s}, {11 * s, 28 * s}, {5 * s, 48 * s},
        {0 * s, 58 * s}, {-5 * s, 48 * s}, {-11 * s, 28 * s}, {-15 * s, 4 * s}
    };
    drawPolygon(deck, 13);

    // Stern cross-board so it feels like rear view
    setColor(118, 78, 42);
    drawLine(-14 * s, 50 * s, 14 * s, 50 * s, 2.0f);
    drawLine(-10 * s, 58 * s, 10 * s, 58 * s, 1.6f);

    // Inner highlight
    setColor(214, 156, 90);
    float deckLight[][2] = {
        {-8 * s, -16 * s}, {-4 * s, -28 * s}, {0 * s, -31 * s},
        {4 * s, -28 * s}, {8 * s, -16 * s}, {8 * s, 4 * s},
        {4 * s, 20 * s}, {0 * s, 30 * s}, {-4 * s, 20 * s}, {-8 * s, 4 * s}
    };
    drawPolygon(deckLight, 10);

    // Side rails
    setColor(110, 76, 44);
    drawLine(-16 * s, -22 * s, -12 * s, 52 * s, 1.4f);
    drawLine(16 * s, -22 * s, 12 * s, 52 * s, 1.4f);

    // Floor planks
    setColor(128, 84, 46);
    drawLine(-10 * s, -12 * s, 10 * s, -12 * s, 1.0f);
    drawLine(-10 * s, 4 * s, 10 * s, 4 * s, 1.0f);
    drawLine(-9 * s, 22 * s, 9 * s, 22 * s, 1.0f);

    // Bamboo mast
    setColor(122, 96, 52);
    drawFilledRect(-1.6f * s, -2 * s, 1.6f * s, 82 * s);

    // Triangular sail slightly turned so it still reads from behind
    setColor(210, 42, 42);
    float sail[][2] = {
        {0 * s, 80 * s}, {-1 * s, 16 * s}, {18 * s, 22 * s},
        {30 * s, 40 * s}, {24 * s, 62 * s}
    };
    drawPolygon(sail, 5);

    setColor(150, 24, 24);
    float sailShade[][2] = {
        {0 * s, 80 * s}, {10 * s, 68 * s}, {14 * s, 48 * s},
        {10 * s, 28 * s}, {-1 * s, 16 * s}
    };
    drawPolygon(sailShade, 5);

    // Boom and rope
    setColor(150, 116, 72);
    drawLine(-1 * s, 20 * s, 18 * s, 24 * s, 1.2f);
    setColor(182, 172, 150);
    drawLine(0 * s, 80 * s, 0 * s, 12 * s, 1.0f);
    drawLine(0 * s, 46 * s, 18 * s, 24 * s, 0.8f);

    // Small flag
    setColor(212, 48, 48);
    float flag[][2] = {
        {1.6f * s, 82 * s}, {12 * s, 78 * s}, {1.6f * s, 72 * s}
    };
    drawPolygon(flag, 3);

    // Water reflections
    setColor(170, 224, 242);
    drawEllipse(10 * s, -16 * s, 5.5f * s, 1.5f * s, 28);
    drawEllipse(-8 * s, 8 * s, 4.5f * s, 1.3f * s, 28);

    glPopMatrix();
}

void drawForegroundTrees()
{
    drawRoundTree(72, 271, 1.6f);
    drawRoundTree(704, 15, 1.9f);

    drawRoundTree(180, 42, 1.0f);
    drawRoundTree(300, 55, 0.95f);
    drawRoundTree(565, 42, 1.05f);
    drawRoundTree(780, 60, 1.0f);
    drawRoundTree(930, 42, 0.92f);

    // extra big trees on open land, clear of road and river
    drawBigRoundTree(144, 310, 1.15f);
    drawBigRoundTree(266, 308, 1.05f);
    drawBigRoundTree(610, 612, 0.82f);
    drawBigRoundTree(708, 604, 0.76f);
    // Removed additional tree near the boat-end area
    drawBigRoundTree(1320, 582, 0.88f);
}

// NEW: nodir dan pashe gach + blue building
void drawRiversideBlueBuildingAndTrees()
{
    const float dx = 130.0f; // shifted more to the right to avoid overlap

    // small grass bank near river
    setColor(190, 224, 150);
    drawEllipse(1188 + dx, 255, 86, 28, 64);
    drawEllipse(1252 + dx, 230, 78, 24, 64);

    // riverside blue building body
    setColor(66, 142, 214);
    drawFilledRect(1188 + dx, 246, 1260 + dx, 356);

    setColor(48, 112, 178);
    float side[][2] = {
        {1260 + dx, 246}, {1278 + dx, 258}, {1278 + dx, 368}, {1260 + dx, 356}
    };
    drawPolygon(side, 4);

    setColor(38, 94, 160);
    drawFilledRect(1196 + dx, 246, 1212 + dx, 356);
    drawFilledRect(1234 + dx, 246, 1250 + dx, 356);

    setColor(90, 168, 232);
    drawFilledRect(1192 + dx, 334, 1256 + dx, 347);
    drawFilledRect(1192 + dx, 306, 1256 + dx, 319);
    drawFilledRect(1192 + dx, 278, 1256 + dx, 291);

    setColor(210, 230, 240);
    drawFilledRect(1190 + dx, 356, 1260 + dx, 363);

    drawWindow(1216 + dx, 330, 1230 + dx, 346);
    drawWindow(1216 + dx, 302, 1230 + dx, 318);
    drawWindow(1216 + dx, 274, 1230 + dx, 290);

    drawDoor(1220 + dx, 246, 1234 + dx, 272, 94, 66, 46);

    // surrounding riverside trees
    drawBigRoundTree(1148 + dx, 214, 0.82f);
    drawBigRoundTree(1168 + dx, 258, 0.72f);
    drawRoundTree(1288 + dx, 246, 0.98f);
    drawRoundTree(1308 + dx, 216, 0.88f);
    drawRoundTree(1278 + dx, 292, 0.86f);
}

// UPDATED: road er duipashe aro beshi serial Christmas tree
void drawRoadsideChristmasTrees()
{
    const int count = 8;
    const float startY = 58.0f;
    const float endY = 462.0f;

    for (int i = 0; i < count; i++)
    {
        float t = (float)i / (float)(count - 1);
        float y = lerp(startY, endY, t);
        float scale = lerp(1.05f, 0.56f, y / VROAD_STRAIGHT_END_Y);

        float centerX = perspectiveRoadCenterX(y);
        float roadW = perspectiveRoadWidth(y);
        float edgeGap = lerp(46.0f, 24.0f, y / VROAD_STRAIGHT_END_Y);

        float leftTreeX = centerX - roadW * 0.5f - edgeGap;
        float rightTreeX = centerX + roadW * 0.5f + edgeGap;

        drawChristmasTree(leftTreeX, y - 6.0f, scale);
        drawChristmasTree(rightTreeX, y - 6.0f, scale);
    }

    // curve section er satheo kichu tree
    for (int i = 0; i < 6; i++)
    {
        float t = 0.05f + i * 0.15f;
        Vec2 p = bezierPoint(t);
        Vec2 tan = normalizeVec(bezierTangent(t));
        Vec2 n = makeVec(-tan.y, tan.x);

        float s = lerp(0.62f, 0.42f, t);
        float offset = turnCurveWidth(t) * 0.5f + lerp(28.0f, 18.0f, t);

        Vec2 leftPos = addVec(p, mulVec(n, offset));
        Vec2 rightPos = subVec(p, mulVec(n, offset));

        drawChristmasTree(leftPos.x, leftPos.y - 10.0f, s);
        drawChristmasTree(rightPos.x, rightPos.y - 10.0f, s);
    }

    // horizontal road er upor serial trees
    const float roadTreeX[] = {520, 610, 700, 790, 880, 970, 1060, 1140};
    const float roadTreeS[] = {0.58f, 0.55f, 0.52f, 0.49f, 0.46f, 0.42f, 0.38f, 0.34f};

    for (int i = 0; i < 8; i++)
    {
        float x = roadTreeX[i];
        float w = turnedRoadWidth(x);
        float gap = lerp(26.0f, 16.0f, (x - HROAD_STRAIGHT_START_X) / (HROAD_END_X - HROAD_STRAIGHT_START_X));

        // Keep the far-right house area fully clear of overlapping Christmas trees.
        if (i <= 4)
            drawChristmasTree(x, HROAD_CENTER_Y + w * 0.5f + gap, roadTreeS[i]);

        if (i <= 4)
            drawChristmasTree(x, HROAD_CENTER_Y - w * 0.5f - gap - 42.0f * roadTreeS[i], roadTreeS[i]);
    }
}



void drawNightOverlay()
{
    setColor(12, 18, 42);
    drawFilledRect(0, 0, 1440, 780);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4ub(8, 16, 36, 110);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(1440, 0);
    glVertex2f(1440, 780);
    glVertex2f(0, 780);
    glEnd();

    glDisable(GL_BLEND);
}

void drawNightSceneTint()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // stronger final darkening so the whole scene feels like night
    glColor4ub(6, 10, 24, 128);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(1440, 0);
    glVertex2f(1440, 780);
    glVertex2f(0, 780);
    glEnd();

    // slightly deeper tint near the lower half for roads, houses, trees and river bank
    glColor4ub(4, 8, 18, 74);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(1440, 0);
    glVertex2f(1440, 420);
    glVertex2f(0, 420);
    glEnd();

    glDisable(GL_BLEND);
}

void drawNightLampsHighlight()
{
    // roadside lamps
    drawStreetLight(355, 455, 65);
    drawStreetLight(548, 585, 55);
    drawStreetLight(740, 578, 60);
    drawStreetLight(915, 574, 58);

    // flyover lamps
    drawFlyoverLamps();
}


void drawTwinklingStar(float x, float y, float phaseOffset)
{
    float a = (sin(starTwinklePhase + phaseOffset) + 1.0f) * 0.5f; // 0..1
    float r = 1.2f + a * 1.8f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4ub(255, 255, 240, (unsigned char)(70 + a * 80));
    drawCircle(x, y, r + 1.4f, 12);

    glColor4ub(255, 250, 220, (unsigned char)(140 + a * 80));
    drawCircle(x, y, r, 12);

    glDisable(GL_BLEND);
}

void drawMoonAndStars()
{
    // stars
    const float stars[][2] = {
        {86, 704}, {128, 742}, {182, 690}, {244, 730}, {316, 706}, {384, 752},
        {428, 694}, {500, 726}, {566, 748}, {624, 712}, {688, 738}, {744, 702},
        {808, 750}, {872, 716}, {934, 742}, {996, 700}, {1058, 732}, {1120, 748},
        {1188, 710}, {1246, 738}, {1310, 700}, {1372, 728}, {210, 654}, {330, 666},
        {462, 650}, {596, 670}, {718, 646}, {846, 662}, {982, 654}, {1116, 666},
        {1268, 648}
    };

    for (int i = 0; i < 31; i++)
    {
        drawTwinklingStar(stars[i][0], stars[i][1], i * 0.47f);
    }

    // moon + glow
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4ub(255, 248, 210, 50);
    drawCircle(1186, 684, 48, 50);
    glColor4ub(255, 248, 220, 90);
    drawCircle(1186, 684, 36, 50);

    glDisable(GL_BLEND);

    setColor(255, 252, 228);
    drawCircle(1186, 684, 24, 60);
    setColor(230, 232, 212);
    drawCircle(1195, 691, 6, 20);
    drawCircle(1174, 674, 4, 18);
}
void drawSky()
{
    if (isNight)
    {
        drawNightOverlay();
    }
    else
    {
        setColor(136, 204, 233);
        drawFilledRect(0, 0, 1440, 780);
    }
}

void drawRailLine()
{
    setColor(95, 95, 95);
    drawFilledRect(0, 255, 1440, 295);

    setColor(70, 70, 70);
    drawLine(0, 265, 1440, 265, 3);
    drawLine(0, 285, 1440, 285, 3);

    setColor(125, 80, 35);
    for (int x = 0; x < 1440; x += 34)
        drawFilledRect((float)x, 263, (float)x + 20, 287);
}

void drawMetroTrain(float x, float y, float s)
{
    setColor(214, 216, 220);
    drawFilledRect(x - 338 * s, y + 10 * s, x - 300 * s, y + 50 * s);

    setColor(224, 226, 230);
    drawFilledRect(x - 300 * s, y + 6 * s, x + 180 * s, y + 54 * s);

    setColor(205, 208, 212);
    drawFilledRect(x - 285 * s, y + 54 * s, x + 160 * s, y + 66 * s);

    setColor(214, 216, 220);
    drawFilledRect(x + 180 * s, y + 10 * s, x + 220 * s, y + 50 * s);

    setColor(0, 130, 215);
    drawFilledRect(x - 338 * s, y + 16 * s, x + 220 * s, y + 21 * s);

    setColor(0, 88, 170);
    drawFilledRect(x - 338 * s, y + 11 * s, x + 220 * s, y + 14 * s);

    setColor(35, 50, 65);
    drawFilledRect(x + 186 * s, y + 22 * s, x + 214 * s, y + 44 * s);

    setColor(35, 50, 65);
    drawFilledRect(x - 332 * s, y + 20 * s, x - 306 * s, y + 42 * s);

    setColor(45, 55, 72);
    drawFilledRect(x - 280 * s, y + 26 * s, x + 170 * s, y + 46 * s);

    for (int i = 0; i < 5; i++)
    {
        float wx = x - 255 * s + i * 78 * s;

        setColor(165, 210, 235);
        drawFilledRect(wx, y + 29 * s, wx + 36 * s, y + 44 * s);

        if (i < 4)
        {
            float dx = wx + 42 * s;
            setColor(235, 238, 242);
            drawFilledRect(dx, y + 18 * s, dx + 24 * s, y + 49 * s);

            setColor(170, 175, 180);
            drawLine(dx + 12 * s, y + 18 * s, dx + 12 * s, y + 49 * s, 1.2f);
        }
    }

    setColor(170, 170, 170);
    drawLine(x - 145 * s, y + 8 * s, x - 145 * s, y + 54 * s, 1.5f);
    drawLine(x + 10 * s, y + 8 * s, x + 10 * s, y + 54 * s, 1.5f);

    setColor(255, 245, 180);
    drawCircle(x + 218 * s, y + 24 * s, 2.5f * s);
    drawCircle(x + 218 * s, y + 36 * s, 2.5f * s);

    setColor(220, 45, 45);
    drawCircle(x - 336 * s, y + 24 * s, 2.5f * s);
    drawCircle(x - 336 * s, y + 36 * s, 2.5f * s);

    for (int i = 0; i < 6; i++)
    {
        float wx = x - 255 * s + i * 90 * s;
        drawWheel(wx, y + 4 * s, 5.5f * s);
    }
}

void drawHorizontalCar(float x, float y, float s,
                       unsigned char r, unsigned char g, unsigned char b, bool right)
{
    if (right)
    {
        setColor(r, g, b);
        float body[][2] = {
            {x + 2 * s, y + 10 * s}, {x + 18 * s, y + 10 * s}, {x + 28 * s, y + 23 * s},
            {x + 42 * s, y + 34 * s}, {x + 78 * s, y + 34 * s}, {x + 96 * s, y + 27 * s},
            {x + 108 * s, y + 18 * s}, {x + 114 * s, y + 18 * s}, {x + 114 * s, y + 12 * s},
            {x + 106 * s, y + 9 * s}, {x + 12 * s, y + 9 * s}
        };
        drawPolygon(body, 11);

        setColor((unsigned char)(r > 24 ? r - 24 : 0),
                 (unsigned char)(g > 24 ? g - 24 : 0),
                 (unsigned char)(b > 24 ? b - 24 : 0));
        drawFilledRect(x + 18 * s, y + 9 * s, x + 98 * s, y + 13 * s);

        setColor((unsigned char)(r < 230 ? r + 25 : 255),
                 (unsigned char)(g < 230 ? g + 25 : 255),
                 (unsigned char)(b < 230 ? b + 25 : 255));
        drawLine(x + 30 * s, y + 33 * s, x + 83 * s, y + 33 * s, 1.2f);

        setColor(188, 224, 246);
        float frontWin[][2] = {
            {x + 31 * s, y + 23 * s}, {x + 43 * s, y + 32 * s},
            {x + 57 * s, y + 32 * s}, {x + 57 * s, y + 23 * s}
        };
        drawPolygon(frontWin, 4);
        float rearWin[][2] = {
            {x + 61 * s, y + 23 * s}, {x + 61 * s, y + 32 * s},
            {x + 79 * s, y + 32 * s}, {x + 90 * s, y + 24 * s}
        };
        drawPolygon(rearWin, 4);

        setColor(70, 80, 92);
        drawLine(x + 59 * s, y + 23 * s, x + 59 * s, y + 32 * s, 1.0f);

        setColor(45, 45, 48);
        drawFilledRect(x + 8 * s, y + 8 * s, x + 22 * s, y + 10 * s);
        drawFilledRect(x + 93 * s, y + 8 * s, x + 108 * s, y + 10 * s);

        setColor(245, 245, 220);
        drawCircle(x + 111 * s, y + 16 * s, 2.0f * s);
        setColor(215, 40, 40);
        drawCircle(x + 5 * s, y + 14 * s, 2.0f * s);

        drawWheel(x + 25 * s, y + 8 * s, 7.5f * s);
        drawWheel(x + 87 * s, y + 8 * s, 7.5f * s);
    }
    else
    {
        setColor(r, g, b);
        float body[][2] = {
            {x + 112 * s, y + 10 * s}, {x + 96 * s, y + 10 * s}, {x + 86 * s, y + 23 * s},
            {x + 72 * s, y + 34 * s}, {x + 36 * s, y + 34 * s}, {x + 18 * s, y + 27 * s},
            {x + 6 * s, y + 18 * s}, {x, y + 18 * s}, {x, y + 12 * s},
            {x + 8 * s, y + 9 * s}, {x + 102 * s, y + 9 * s}
        };
        drawPolygon(body, 11);

        setColor((unsigned char)(r > 24 ? r - 24 : 0),
                 (unsigned char)(g > 24 ? g - 24 : 0),
                 (unsigned char)(b > 24 ? b - 24 : 0));
        drawFilledRect(x + 16 * s, y + 9 * s, x + 96 * s, y + 13 * s);

        setColor((unsigned char)(r < 230 ? r + 25 : 255),
                 (unsigned char)(g < 230 ? g + 25 : 255),
                 (unsigned char)(b < 230 ? b + 25 : 255));
        drawLine(x + 31 * s, y + 33 * s, x + 84 * s, y + 33 * s, 1.2f);

        setColor(188, 224, 246);
        float frontWin[][2] = {
            {x + 23 * s, y + 24 * s}, {x + 34 * s, y + 32 * s},
            {x + 52 * s, y + 32 * s}, {x + 52 * s, y + 23 * s}
        };
        drawPolygon(frontWin, 4);
        float rearWin[][2] = {
            {x + 56 * s, y + 23 * s}, {x + 56 * s, y + 32 * s},
            {x + 70 * s, y + 32 * s}, {x + 82 * s, y + 23 * s}
        };
        drawPolygon(rearWin, 4);

        setColor(70, 80, 92);
        drawLine(x + 54 * s, y + 23 * s, x + 54 * s, y + 32 * s, 1.0f);

        setColor(45, 45, 48);
        drawFilledRect(x + 6 * s, y + 8 * s, x + 21 * s, y + 10 * s);
        drawFilledRect(x + 92 * s, y + 8 * s, x + 106 * s, y + 10 * s);

        setColor(245, 245, 220);
        drawCircle(x + 3 * s, y + 16 * s, 2.0f * s);
        setColor(215, 40, 40);
        drawCircle(x + 107 * s, y + 14 * s, 2.0f * s);

        drawWheel(x + 27 * s, y + 8 * s, 7.5f * s);
        drawWheel(x + 89 * s, y + 8 * s, 7.5f * s);
    }
}

void drawBus(float x, float y, float s,
             unsigned char r, unsigned char g, unsigned char b, bool right)
{
    setColor(0, 0, 0);
    drawEllipse(x + 69 * s, y + 7 * s, 60 * s, 3.2f * s, 36);

    if (right)
    {
        setColor(r, g, b);
        float body[][2] = {
            {x + 4 * s, y + 10 * s}, {x + 20 * s, y + 10 * s}, {x + 30 * s, y + 14 * s},
            {x + 118 * s, y + 14 * s}, {x + 132 * s, y + 20 * s}, {x + 136 * s, y + 32 * s},
            {x + 132 * s, y + 46 * s}, {x + 10 * s, y + 46 * s}, {x + 4 * s, y + 38 * s}
        };
        drawPolygon(body, 9);

        setColor((unsigned char)(r < 235 ? r + 20 : 255),
                 (unsigned char)(g < 235 ? g + 20 : 255),
                 (unsigned char)(b < 235 ? b + 20 : 255));
        drawFilledRect(x + 16 * s, y + 42 * s, x + 122 * s, y + 45 * s);

        setColor((unsigned char)(r > 28 ? r - 28 : 0),
                 (unsigned char)(g > 28 ? g - 28 : 0),
                 (unsigned char)(b > 28 ? b - 28 : 0));
        drawFilledRect(x + 14 * s, y + 11 * s, x + 124 * s, y + 15 * s);

        setColor(178, 220, 245);
        for (int i = 0; i < 5; i++)
        {
            float wx = x + 19 * s + i * 19 * s;
            drawFilledRect(wx, y + 25 * s, wx + 14 * s, y + 39 * s);
        }
        float frontGlass[][2] = {
            {x + 116 * s, y + 23 * s}, {x + 131 * s, y + 23 * s},
            {x + 132 * s, y + 40 * s}, {x + 120 * s, y + 40 * s}
        };
        drawPolygon(frontGlass, 4);

        setColor(235, 238, 242);
        drawFilledRect(x + 101 * s, y + 15 * s, x + 116 * s, y + 41 * s);
        setColor(82, 88, 98);
        drawLine(x + 108.5f * s, y + 15 * s, x + 108.5f * s, y + 41 * s, 1.0f);

        setColor(255, 214, 68);
        drawLine(x + 12 * s, y + 21 * s, x + 100 * s, y + 21 * s, 1.6f);

        setColor(245, 245, 215);
        drawCircle(x + 135 * s, y + 18 * s, 2.2f * s);
        drawCircle(x + 135 * s, y + 25 * s, 2.0f * s);
        setColor(215, 40, 40);
        drawCircle(x + 5 * s, y + 18 * s, 2.1f * s);

        drawWheel(x + 28 * s, y + 8 * s, 7.8f * s);
        drawWheel(x + 107 * s, y + 8 * s, 7.8f * s);
    }
    else
    {
        setColor(r, g, b);
        float body[][2] = {
            {x + 134 * s, y + 10 * s}, {x + 118 * s, y + 10 * s}, {x + 108 * s, y + 14 * s},
            {x + 20 * s, y + 14 * s}, {x + 6 * s, y + 20 * s}, {x + 2 * s, y + 32 * s},
            {x + 6 * s, y + 46 * s}, {x + 128 * s, y + 46 * s}, {x + 134 * s, y + 38 * s}
        };
        drawPolygon(body, 9);

        setColor((unsigned char)(r < 235 ? r + 20 : 255),
                 (unsigned char)(g < 235 ? g + 20 : 255),
                 (unsigned char)(b < 235 ? b + 20 : 255));
        drawFilledRect(x + 16 * s, y + 42 * s, x + 122 * s, y + 45 * s);

        setColor((unsigned char)(r > 28 ? r - 28 : 0),
                 (unsigned char)(g > 28 ? g - 28 : 0),
                 (unsigned char)(b > 28 ? b - 28 : 0));
        drawFilledRect(x + 12 * s, y + 11 * s, x + 122 * s, y + 15 * s);

        setColor(178, 220, 245);
        for (int i = 0; i < 5; i++)
        {
            float wx = x + 30 * s + i * 19 * s;
            drawFilledRect(wx, y + 25 * s, wx + 14 * s, y + 39 * s);
        }
        float frontGlass[][2] = {
            {x + 5 * s, y + 23 * s}, {x + 20 * s, y + 23 * s},
            {x + 17 * s, y + 40 * s}, {x + 4 * s, y + 40 * s}
        };
        drawPolygon(frontGlass, 4);

        setColor(235, 238, 242);
        drawFilledRect(x + 22 * s, y + 15 * s, x + 37 * s, y + 41 * s);
        setColor(82, 88, 98);
        drawLine(x + 29.5f * s, y + 15 * s, x + 29.5f * s, y + 41 * s, 1.0f);

        setColor(255, 214, 68);
        drawLine(x + 38 * s, y + 21 * s, x + 125 * s, y + 21 * s, 1.6f);

        setColor(245, 245, 215);
        drawCircle(x + 3 * s, y + 18 * s, 2.2f * s);
        drawCircle(x + 3 * s, y + 25 * s, 2.0f * s);
        setColor(215, 40, 40);
        drawCircle(x + 131 * s, y + 18 * s, 2.1f * s);

        drawWheel(x + 30 * s, y + 8 * s, 7.8f * s);
        drawWheel(x + 109 * s, y + 8 * s, 7.8f * s);
    }
}

void drawTruck(float x, float y, float s,
               unsigned char r, unsigned char g, unsigned char b, bool right)
{
    setColor(0, 0, 0);
    drawEllipse(x + 68 * s, y + 7 * s, 56 * s, 3.0f * s, 36);

    if (right)
    {
        setColor(216, 219, 224);
        float cab[][2] = {
            {x + 4 * s, y + 12 * s}, {x + 28 * s, y + 12 * s}, {x + 38 * s, y + 21 * s},
            {x + 38 * s, y + 42 * s}, {x + 8 * s, y + 42 * s}, {x + 4 * s, y + 34 * s}
        };
        drawPolygon(cab, 6);
        setColor(180, 220, 245);
        float glass[][2] = {
            {x + 9 * s, y + 24 * s}, {x + 24 * s, y + 24 * s},
            {x + 30 * s, y + 37 * s}, {x + 12 * s, y + 37 * s}
        };
        drawPolygon(glass, 4);

        setColor(r, g, b);
        drawFilledRect(x + 35 * s, y + 16 * s, x + 113 * s, y + 42 * s);
        setColor((unsigned char)(r > 28 ? r - 28 : 0),
                 (unsigned char)(g > 28 ? g - 28 : 0),
                 (unsigned char)(b > 28 ? b - 28 : 0));
        drawFilledRect(x + 87 * s, y + 19 * s, x + 113 * s, y + 42 * s);
        setColor((unsigned char)(r < 230 ? r + 20 : 255),
                 (unsigned char)(g < 230 ? g + 20 : 255),
                 (unsigned char)(b < 230 ? b + 20 : 255));
        drawLine(x + 42 * s, y + 39 * s, x + 103 * s, y + 39 * s, 1.1f);

        setColor(88, 88, 92);
        drawFilledRect(x + 31 * s, y + 14 * s, x + 35 * s, y + 20 * s);
        drawFilledRect(x + 113 * s, y + 14 * s, x + 118 * s, y + 18 * s);

        setColor(245, 245, 215);
        drawCircle(x + 4 * s, y + 18 * s, 2.2f * s);
        setColor(215, 40, 40);
        drawCircle(x + 111 * s, y + 18 * s, 2.2f * s);

        drawWheel(x + 19 * s, y + 8 * s, 7.0f * s);
        drawWheel(x + 72 * s, y + 8 * s, 7.0f * s);
        drawWheel(x + 101 * s, y + 8 * s, 7.0f * s);
    }
    else
    {
        setColor(216, 219, 224);
        float cab[][2] = {
            {x + 134 * s, y + 12 * s}, {x + 110 * s, y + 12 * s}, {x + 100 * s, y + 21 * s},
            {x + 100 * s, y + 42 * s}, {x + 130 * s, y + 42 * s}, {x + 134 * s, y + 34 * s}
        };
        drawPolygon(cab, 6);
        setColor(180, 220, 245);
        float glass[][2] = {
            {x + 125 * s, y + 24 * s}, {x + 110 * s, y + 24 * s},
            {x + 104 * s, y + 37 * s}, {x + 122 * s, y + 37 * s}
        };
        drawPolygon(glass, 4);

        setColor(r, g, b);
        drawFilledRect(x + 21 * s, y + 16 * s, x + 99 * s, y + 42 * s);
        setColor((unsigned char)(r > 28 ? r - 28 : 0),
                 (unsigned char)(g > 28 ? g - 28 : 0),
                 (unsigned char)(b > 28 ? b - 28 : 0));
        drawFilledRect(x + 21 * s, y + 19 * s, x + 47 * s, y + 42 * s);
        setColor((unsigned char)(r < 230 ? r + 20 : 255),
                 (unsigned char)(g < 230 ? g + 20 : 255),
                 (unsigned char)(b < 230 ? b + 20 : 255));
        drawLine(x + 31 * s, y + 39 * s, x + 92 * s, y + 39 * s, 1.1f);

        setColor(88, 88, 92);
        drawFilledRect(x + 99 * s, y + 14 * s, x + 103 * s, y + 20 * s);
        drawFilledRect(x + 15 * s, y + 14 * s, x + 21 * s, y + 18 * s);

        setColor(245, 245, 215);
        drawCircle(x + 135 * s, y + 18 * s, 2.2f * s);
        setColor(215, 40, 40);
        drawCircle(x + 23 * s, y + 18 * s, 2.2f * s);

        drawWheel(x + 37 * s, y + 8 * s, 7.0f * s);
        drawWheel(x + 66 * s, y + 8 * s, 7.0f * s);
        drawWheel(x + 119 * s, y + 8 * s, 7.0f * s);
    }
}

void drawTopViewCarBase(float s,
                        unsigned char r, unsigned char g, unsigned char b)
{
    setColor(r, g, b);
    drawFilledRect(-10 * s, -20 * s, 10 * s, 20 * s);

    setColor(
        (unsigned char)(r > 20 ? r - 20 : 0),
        (unsigned char)(g > 20 ? g - 20 : 0),
        (unsigned char)(b > 20 ? b - 20 : 0));
    drawFilledRect(-8 * s, 10 * s, 8 * s, 20 * s);

    setColor(
        (unsigned char)(r > 35 ? r - 35 : 0),
        (unsigned char)(g > 35 ? g - 35 : 0),
        (unsigned char)(b > 35 ? b - 35 : 0));
    drawFilledRect(-8 * s, -20 * s, 8 * s, -10 * s);

    setColor(180, 220, 245);
    drawFilledRect(-7 * s, 2 * s, 7 * s, 10 * s);
    drawFilledRect(-7 * s, -10 * s, 7 * s, -2 * s);

    setColor(150, 150, 150);
    drawFilledRect(-1 * s, -10 * s, 1 * s, 10 * s);

    setColor(30, 30, 30);
    drawFilledRect(-12 * s, -14 * s, -10 * s, -6 * s);
    drawFilledRect(10 * s, -14 * s, 12 * s, -6 * s);
    drawFilledRect(-12 * s, 6 * s, -10 * s, 14 * s);
    drawFilledRect(10 * s, 6 * s, 12 * s, 14 * s);

    setColor(255, 245, 180);
    drawFilledRect(-6 * s, 19 * s, -2 * s, 20 * s);
    drawFilledRect(2 * s, 19 * s, 6 * s, 20 * s);

    setColor(220, 40, 40);
    drawFilledRect(-6 * s, -20 * s, -2 * s, -19 * s);
    drawFilledRect(2 * s, -20 * s, 6 * s, -19 * s);
}

void drawTopViewCarRotated(float x, float y, float s,
                           unsigned char r, unsigned char g, unsigned char b,
                           float angleDeg)
{
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glRotatef(angleDeg, 0.0f, 0.0f, 1.0f);
    drawTopViewCarBase(s, r, g, b);
    glPopMatrix();
}

// ------------------------------
// Smooth bottom car path
// ------------------------------
/*
Algorithm: Piecewise parametric motion
Use: Car movement is split into three parts: straight road, Bezier turn, and
     horizontal road. Each part computes position, angle, and scale differently.
*/
void getBottomCarState(float t, float &x, float &y, float &angleDeg, float &scale)
{
    // keep motion visually uniform across straight, curve, and horizontal parts
    const float t1 = 0.68f;
    const float t2 = 0.84f;

    if (t <= t1)
    {
        float u = t / t1;
        y = lerp(-50.0f, VROAD_STRAIGHT_END_Y, u);
        x = perspectiveRoadCenterX(y);

        float roadW = perspectiveRoadWidth(y);
        float laneOffset = roadW * CAR_LANE_RATIO_VERTICAL;
        x += laneOffset;

        angleDeg = 0.0f;
        scale = lerp(1.45f, 0.52f, u);
    }
    else if (t <= t2)
    {
        float u = (t - t1) / (t2 - t1);

        Vec2 p = bezierPoint(u);
        Vec2 tan = normalizeVec(bezierTangent(u));
        Vec2 n = makeVec(-tan.y, tan.x);

        float roadW = turnCurveWidth(u);
        float safeLaneOffset = roadW * CAR_LANE_RATIO_CURVE;
        Vec2 q = addVec(p, mulVec(n, safeLaneOffset));

        x = q.x;
        y = q.y;

        angleDeg = atan2(tan.y, tan.x) * 180.0f / PI - 90.0f;
        scale = lerp(0.52f, 0.38f, u);
    }
    else
    {
        float u = clamp01((t - t2) / (1.0f - t2));

        x = lerp(HROAD_STRAIGHT_START_X + 8.0f, HROAD_END_X, u);
        y = HROAD_CENTER_Y;

        float w = turnedRoadWidth(x);
        y += w * CAR_LANE_RATIO_HORIZONTAL;

        angleDeg = -90.0f;
        scale = lerp(0.38f, 0.22f, u);
    }
}


/*
Technique: Layered rendering + region clipping
Use: Vehicle light overlays are rendered in restricted regions so they appear
     inside the train lane/flyover only.
*/
void drawNightVehicleLights()
{
    if (!isNight) return;

    // Train headlights stay only inside the train line band
    beginClipRect(0, 255, 1440, 40);
    drawTrainHeadlightsOverlay(trainX, 267, 0.9f);
    endClipRect();

    // Flyover vehicle headlights stay only inside the flyover road band
    beginClipRect(0, 170, 1440, 65);
    drawCarLightsOverlay(flyoverRightLaneX[0], 174, 0.84f, false);
    drawBusLightsOverlay(flyoverRightLaneX[1], 169, 0.74f, false);
    drawTruckLightsOverlay(flyoverRightLaneX[2], 170, 0.76f, true);

    drawCarLightsOverlay(flyoverLeftLaneX[0], 202, 0.86f, true);
    drawBusLightsOverlay(flyoverLeftLaneX[1], 197, 0.74f, true);
    drawTruckLightsOverlay(flyoverLeftLaneX[2], 198, 0.76f, false);
    endClipRect();
}


/*
Technique: Layered rendering + depth illusion
Use: Bottom-road headlights are clipped below upper layers so they look like
     they pass underneath the rail/flyover structures.
*/
void drawNightBottomRoadVehicleLights()
{
    if (!isNight) return;

    // Keep the lower-road headlights below the rail/flyover layers,
    // so they appear to pass underneath instead of over them.
    beginClipRect(0, 0, 1440, 255);

    for (int i = 0; i < 5; i++)
    {
        float x, y, angleDeg, s;
        getBottomCarState(bottomCarsT[i], x, y, angleDeg, s);
        drawTopViewCarLightsOverlay(x, y, s, angleDeg);
    }

    endClipRect();
}


void drawNightHouseLights()
{
    if (!isNight) return;

    // main houses
    drawHouseFrontLight(220.0f, 334.0f, 34.0f, 17.0f);   // apartment left
    drawHouseFrontLight(557.0f, 352.0f, 30.0f, 15.0f);   // blue building
    drawHouseFrontLight(698.0f, 432.0f, 27.0f, 13.5f);   // cream house center
    drawHouseFrontLight(1110.0f, 432.0f, 32.0f, 16.0f);  // big house right
    drawHouseFrontLight(1199.0f, 336.0f, 29.0f, 14.0f);  // right cream house

    // small village houses on the hills


    // riverside blue building

}

void drawBottomRoadCars()
{
    unsigned char colors[5][3] = {
        {210, 45, 45},
        {30, 110, 210},
        {30, 160, 80},
        {235, 160, 35},
        {150, 70, 180}
    };

    for (int i = 0; i < 5; i++)
    {
        float x, y, angleDeg, s;
        getBottomCarState(bottomCarsT[i], x, y, angleDeg, s);
        drawTopViewCarRotated(x, y, s, colors[i][0], colors[i][1], colors[i][2], angleDeg);
    }
}

void drawFlyoverCars()
{
    // upper lane: vehicles move from right to left
    drawHorizontalCar(flyoverRightLaneX[0], 174, 0.84f, 220, 40, 40, false);
    drawBus(flyoverRightLaneX[1], 169, 0.74f, 40, 110, 215, false);
    drawTruck(flyoverRightLaneX[2], 170, 0.76f, 38, 160, 92, true);

    // lower lane: vehicles move from left to right
    drawHorizontalCar(flyoverLeftLaneX[0], 202, 0.86f, 235, 150, 20, true);
    drawBus(flyoverLeftLaneX[1], 197, 0.74f, 138, 68, 178, true);
    drawTruck(flyoverLeftLaneX[2], 198, 0.76f, 70, 138, 205, false);
}

/*
Algorithm block: Frame-by-frame animation update loop
Contains:
- position integration (x += speed)
- cyclic wrapping for repeated objects
- phase updates for sway/twinkle/ripple
- parametric updates for train, cars, and boat
This acts as the core simulation step called every frame.
*/
void update(int)
{
    cloudOffset += 0.18f;
    if (cloudOffset > 1820.0f)
        cloudOffset = 0.0f;

    treeSwayPhase += 0.03f;
    wrapPhase(treeSwayPhase, 2.0f * PI);

    starTwinklePhase += 0.06f;
    wrapPhase(starTwinklePhase, 2.0f * PI);

    riverAnim += 0.0028f;
    if (riverAnim > 1.0f)
        riverAnim -= 1.0f;

    if (trainMoving)
    {
        trainX += 3.2f;
        if (trainX > 1880.0f)
            trainX = -520.0f;
    }

    if (carsMoving)
    {
        for (int i = 0; i < 3; i++)
        {
            flyoverRightLaneX[i] -= 2.2f + i * 0.12f;
            if (flyoverRightLaneX[i] < -240.0f)
                flyoverRightLaneX[i] = 1580.0f;

            flyoverLeftLaneX[i] += 2.0f + i * 0.10f;
            if (flyoverLeftLaneX[i] > 1580.0f)
                flyoverLeftLaneX[i] = -240.0f;
        }
    }

    if (carsMoving)
    {
        for (int i = 0; i < 5; i++)
        {
            bottomCarsT[i] += 0.0028f;
            if (bottomCarsT[i] > 1.0f)
                bottomCarsT[i] = -0.12f;
        }
    }

    if (boatMoving)
    {
        boatT += boatForward ? boatSpeed : -boatSpeed;

        if (boatT >= 1.0f)
        {
            boatT = 1.0f;
            boatForward = false;
        }
        else if (boatT <= 0.0f)
        {
            boatT = 0.0f;
            boatForward = true;
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}


void keyboard(unsigned char key, int, int)
{
    switch (key)
    {
        case 't':
        case 'T':
            trainMoving = !trainMoving;
            break;
        case 'c':
        case 'C':
            carsMoving = !carsMoving;
            break;
        case 'b':
        case 'B':
            boatMoving = !boatMoving;
            break;
        case 'd':
        case 'D':
            isNight = !isNight;
            break;
        default:
            break;
    }

    glutPostRedisplay();
}

/*
Technique: Painter's algorithm / layered scene composition
Use: Objects are drawn in a carefully chosen order so nearer objects appear in
     front of farther ones without needing a full 3D depth buffer approach.
*/
void display()
{
    glClear(GL_COLOR_BUFFER_BIT);

    drawSky();

    if (isNight)
    {
        drawMoonAndStars();
    }
    else
    {
        drawCloud(getWrappedCloudX(633), 718, 1.2f);
        drawCloud(getWrappedCloudX(1065), 718, 1.3f);
        drawCloud(getWrappedCloudX(1393), 717, 1.2f);

        setColor(240, 220, 180);
        drawEllipse(791, 706, 8, 3, 50);
        drawEllipse(778, 713, 7, 2, 50);
        drawEllipse(767, 717, 6, 2, 50);
    }

    drawFarHills();
    drawLandDetails();
    drawRiver();
    drawRiverAnimation();
    drawGrassPatches();
    drawGroundTexture();
    drawWildFlowers();
    drawRoads();

    // Draw under-road vehicles before the riverside/right-side houses,
    // so those houses stay in front and the cars pass behind them.
    drawBottomRoadCars();

    if (isNight)
    {
        drawNightBottomRoadVehicleLights();
    }

    drawRightCreamHouse();
    drawRiversideBlueBuildingAndTrees();
    drawApartmentLeft();
    drawBlueBuilding();
    drawCreamHouseCenter();
    drawWarehouse();
    drawBigHouseRight();
    drawTinyHousesAndFarObjects();

    drawForegroundTrees();
    drawRoadsideChristmasTrees();

    Vec2 boatPos = getBoatPosition(boatT);
    float boatAngle = getBoatAngle(boatT);
    float boatScale = getBoatScale(boatT);
    drawBoat(boatPos.x, boatPos.y, boatScale, boatAngle);

    drawRailLine();
    drawMetroTrain(trainX, 267, 0.9f);

    drawHighway();
    drawFlyoverCars();

    if (isNight)
    {
        drawNightSceneTint();
        drawMoonAndStars();
        drawNightLampsHighlight();
        drawNightVehicleLights();
        drawNightHouseLights();
    }

    glFlush();
    glutSwapBuffers();
}

void init()
{
    glClearColor(1, 1, 1, 1);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, 1440, 0, 780);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(1440, 780);
    glutInitWindowPosition(50, 50);
    glutCreateWindow("Landscape - Metro Train Straight Front");

    init();
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(16, update, 0);
    glutMainLoop();

    return 0;
}