// caves_b4d3_param_hud.c
// Automa cellulare 2D per caverne, con parametri interattivi B/D/Seed.
// Pan/zoom per navigare, HUD overlay fisso con valori leggibili.

#include <GL/glut.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdio.h>

// ====== Parametri griglia ======
#define W 200
#define H 140
static unsigned char g[H][W], nxt[H][W], age[H][W];

// ====== Vista (camera 2D) ======
static int winW=1200, winH=800;
static float camX=W*0.5f, camY=H*0.5f, zoom=1.0f; // centro e zoom
static float xminV,xmaxV,yminV,ymaxV;
static int draggingPan=0, lastX=0, lastY=0;

// ====== Simulazione / UI ======
static int autoplay=0, timerMs=70, showGrid=1;
static int seedPct=35;         // % iniziale di celle vive (roccia)
static int BIRTH_N=4;          // parametro Birth
static int DEATH_N=3;          // parametro Death

// ---- Utils ----
static inline int clampi(int v,int a,int b){ return v<a?a:(v>b?b:v); }
static inline float clampf(float v,float a,float b){ return v<a?a:(v>b?b:v); }

// ---- Conteggio vicini (Moore 8) ----
static inline int nbors(int r,int c){
    int s=0;
    for(int dr=-1; dr<=1; ++dr){
        for(int dc=-1; dc<=1; ++dc){
            if(!dr && !dc) continue;
            int rr=r+dr, cc=c+dc;
            if(rr>=0 && rr<H && cc>=0 && cc<W) s += g[rr][cc];
        }
    }
    return s;
}

// ---- Step Birth/Death ----
static void step(void){
    for(int r=0;r<H;++r){
        for(int c=0;c<W;++c){
            int n=nbors(r,c);
            unsigned char alive=g[r][c], v=alive;
            if(!alive && n==BIRTH_N) v=1;
            else if(alive && n==DEATH_N) v=0;
            nxt[r][c]=v;
        }
    }
    for(int r=0;r<H;++r){
        for(int c=0;c<W;++c){
            g[r][c]=nxt[r][c];
            age[r][c]=g[r][c] ? (unsigned char)(age[r][c] + (age[r][c]<250)) : 0;
        }
    }
    glutPostRedisplay();
}

// ---- Seed/Clear ----
static void randomSeed(int pct){
    pct = clampi(pct, 0, 100);
    for(int r=0;r<H;++r) for(int c=0;c<W;++c){
        g[r][c] = (rand()%100) < pct;
        age[r][c] = g[r][c] ? 1 : 0;
    }
    glutPostRedisplay();
}
static void clearAll(void){
    for(int r=0;r<H;++r) for(int c=0;c<W;++c){ g[r][c]=0; age[r][c]=0; }
    glutPostRedisplay();
}

// ---- Proiezione ----
static void applyProjection(void){
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (float)winW/(float)winH;
    float halfH = (H*0.6f)/zoom;  // 60% dell'altezza griglia visibile a zoom=1
    float halfW = halfH * aspect;
    xminV = camX - halfW; xmaxV = camX + halfW;
    yminV = camY - halfH; ymaxV = camY + halfH;
    gluOrtho2D(xminV,xmaxV,yminV,ymaxV);
}

// ---- Colori ----
static void setRockColor(int r,int c){
    float t = age[r][c]/255.0f;
    float dark=0.35f*t;
    float R=0.50f-dark, G=0.46f-dark, B=0.40f-dark;
    glColor3f(clampf(R,0.08f,0.9f), clampf(G,0.08f,0.9f), clampf(B,0.08f,0.9f));
}
static void setAirColor(int r,int c){
    int n = nbors(r,c);
    float occl=0.06f*n;
    glColor3f(0.07f+0.26f*occl, 0.08f+0.19f*occl, 0.10f+0.14f*occl);
}

// ---- Grid overlay ----
static void drawGridLines(void){
    if(!showGrid) return;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1,1,1,0.09f);
    glBegin(GL_LINES);
    for(int c=0;c<=W;++c){ glVertex2f(c,0); glVertex2f(c,H); }
    for(int r=0;r<=H;++r){ glVertex2f(0,r); glVertex2f(W,r); }
    glEnd();
    glDisable(GL_BLEND);
}

// ---- HUD overlay fisso ----
static void drawHUD(void){
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, winW, 0, winH);   // coordinate pixel finestra
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(1,1,1);
    glRasterPos2i(10, winH - 20);   // margine 10px, 20px dal top
    char buf[128];
    snprintf(buf,sizeof(buf),"Seed:%d%%  Birth:%d  Death:%d  Mode:%s",
             seedPct, BIRTH_N, DEATH_N, autoplay?"AUTO":"MANUAL");
    for(char* p=buf; *p; ++p) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *p);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// ---- Rendering ----
static void display(void){
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    // sfondo
    glColor3f(0.02f,0.02f,0.03f);
    glBegin(GL_QUADS);
    glVertex2f(xminV,yminV); glVertex2f(xmaxV,yminV);
    glVertex2f(xmaxV,ymaxV); glVertex2f(xminV,ymaxV);
    glEnd();

    // celle
    glBegin(GL_QUADS);
    for(int r=0;r<H;++r){
        for(int c=0;c<W;++c){
            if(g[r][c]) setRockColor(r,c); else setAirColor(r,c);
            float x0=c, y0=r, x1=c+1.0f, y1=r+1.0f;
            glVertex2f(x0,y0); glVertex2f(x1,y0); glVertex2f(x1,y1); glVertex2f(x0,y1);
        }
    }
    glEnd();

    drawGridLines();
    drawHUD();

    glutSwapBuffers();
}

// ---- Input ----
static void keyboard(unsigned char k,int,int){
    switch(k){
        case 27: exit(0);
        case ' ': step(); break;
        case 'p': autoplay=!autoplay; break;
        case 'r': randomSeed(seedPct); break;
        case 'c': clearAll(); break;
        case 'g': showGrid=!showGrid; glutPostRedisplay(); break;
        case '+': zoom=fminf(20.0f, zoom*1.1f); applyProjection(); glutPostRedisplay(); break;
        case '-': zoom=fmaxf(0.1f, zoom/1.1f); applyProjection(); glutPostRedisplay(); break;
        case 'w': camY += 5.0f/zoom; applyProjection(); glutPostRedisplay(); break;
        case 's': camY -= 5.0f/zoom; applyProjection(); glutPostRedisplay(); break;
        case 'a': camX -= 5.0f/zoom; applyProjection(); glutPostRedisplay(); break;
        case 'd': camX += 5.0f/zoom; applyProjection(); glutPostRedisplay(); break;
        case '[': seedPct = clampi(seedPct-1,10,60); glutPostRedisplay(); break;
        case ']': seedPct = clampi(seedPct+1,10,60); glutPostRedisplay(); break;
        case 'b': BIRTH_N = clampi(BIRTH_N-1,0,8); glutPostRedisplay(); break;
        case 'n': BIRTH_N = clampi(BIRTH_N+1,0,8); glutPostRedisplay(); break;
        case 'k': DEATH_N = clampi(DEATH_N-1,0,8); glutPostRedisplay(); break;
        case 'l': DEATH_N = clampi(DEATH_N+1,0,8); glutPostRedisplay(); break;
    }
}
static void special(int key,int,int){
    if(key==GLUT_KEY_UP){ camY += 5.0f/zoom; applyProjection(); glutPostRedisplay(); }
    if(key==GLUT_KEY_DOWN){ camY -= 5.0f/zoom; applyProjection(); glutPostRedisplay(); }
    if(key==GLUT_KEY_LEFT){ camX -= 5.0f/zoom; applyProjection(); glutPostRedisplay(); }
    if(key==GLUT_KEY_RIGHT){ camX += 5.0f/zoom; applyProjection(); glutPostRedisplay(); }
}
static void mouse(int btn,int state,int x,int y){
    if(btn==GLUT_LEFT_BUTTON && state==GLUT_DOWN){
        float fx = xminV + (x/(float)winW)*(xmaxV-xminV);
        float fy = yminV + ((winH-1-y)/(float)winH)*(ymaxV-yminV);
        int c=(int)floorf(fx), r=(int)floorf(fy);
        if(r>=0 && r<H && c>=0 && c<W){
            g[r][c]=!g[r][c]; age[r][c]=g[r][c]?1:0;
            glutPostRedisplay();
        }
    }
    if(btn==3 && state==GLUT_DOWN){ zoom=fminf(20.0f, zoom*1.1f); applyProjection(); glutPostRedisplay(); }
    if(btn==4 && state==GLUT_DOWN){ zoom=fmaxf(0.1f, zoom/1.1f); applyProjection(); glutPostRedisplay(); }
    if(btn==GLUT_RIGHT_BUTTON && state==GLUT_DOWN){ draggingPan=1; lastX=x; lastY=y; }
    if(btn==GLUT_RIGHT_BUTTON && state==GLUT_UP)  { draggingPan=0; }
}
static void motion(int x,int y){
    if(!draggingPan) return;
    int dx=x-lastX, dy=y-lastY; lastX=x; lastY=y;
    float dWx = dx * (xmaxV-xminV)/(float)winW;
    float dWy = -dy * (ymaxV-yminV)/(float)winH;
    camX -= dWx; camY -= dWy;
    applyProjection(); glutPostRedisplay();
}

// ---- Reshape & Timer ----
static void reshape(int w,int h){
    winW=w; winH=(h>1?h:1);
    glViewport(0,0,winW,winH);
    applyProjection();
}
static void timer(int){
    if(autoplay) step();
    glutTimerFunc(timerMs,timer,0);
}

// ---- Init ----
static void initGL(void){
    glClearColor(0.02f,0.02f,0.03f,1.0f);
    glDisable(GL_DEPTH_TEST);
    applyProjection();
    randomSeed(seedPct);
}

int main(int argc,char**argv){
    srand((unsigned)time(NULL));
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB);
    glutInitWindowSize(winW,winH);
    glutCreateWindow("Caverne CA - Parametric B/D + Seed (Pan/Zoom)");
    initGL();
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutReshapeFunc(reshape);
    glutTimerFunc(timerMs,timer,0);
    glutMainLoop();
    return 0;
}
