


#include <GL/glut.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#define K 8                 // griglia = 2^K + 1   (es. K=8 -> 257x257)
#define N ((1<<K)+1)
#define ZSCALE 18.0f

static float H[N][N];       // heightmap
static unsigned char UPDATED[N][N];
static int size = N-1;
static int step_len;
static float jitter;
static float roughness = 0.55f;
static int phase = 0;
static int iter_level = 0;
static int autoplay = 0;

static float rotY = -35.0f; // rotazione orizzontale
static float rotX = -35.0f; // rotazione verticale (nuovo)
static float camX=0, camY=120, camZ=320;

// RNG
static inline float frand(float a, float b){ return a + (b-a) * (rand()/(float)RAND_MAX); }

static void clear_updated(){
    for(int y=0;y<=size;y++) for(int x=0;x<=size;x++) UPDATED[y][x]=0;
}

static void reset_heightmap(){
    for(int y=0;y<=size;y++) for(int x=0;x<=size;x++) H[y][x]=0.0f;
    H[0][0]       = frand(-1.0f, 1.0f);
    H[0][size]    = frand(-1.0f, 1.0f);
    H[size][0]    = frand(-1.0f, 1.0f);
    H[size][size] = frand(-1.0f, 1.0f);

    iter_level = 0;
    step_len   = size;
    jitter     = 1.0f;
    phase      = 0;
    clear_updated();
}

static void minmax(float* mn, float* mx){
    *mn=1e9f; *mx=-1e9f;
    for(int y=0;y<=size;y++) for(int x=0;x<=size;x++){
        if(H[y][x]<*mn) *mn=H[y][x];
        if(H[y][x]>*mx) *mx=H[y][x];
    }
}

static void diamond_step(int step, float scale){
    int half = step/2;
    for(int y=half; y<size; y+=step){
        for(int x=half; x<size; x+=step){
            float a = H[y-half][x-half];
            float b = H[y-half][x+half];
            float c = H[y+half][x-half];
            float d = H[y+half][x+half];
            float avg = 0.25f*(a+b+c+d);
            H[y][x] = avg + frand(-scale, scale);
            UPDATED[y][x] = 1;
        }
    }
}

static void square_step(int step, float scale){
    int half = step/2;
    for(int y=0; y<=size; y+=half){
        int start = ((y/half)%2==0) ? half : 0;
        for(int x=start; x<=size; x+=step){
            float sum=0.0f; int cnt=0;
            if(x-half >= 0)    { sum += H[y][x-half]; cnt++; }
            if(x+half <= size) { sum += H[y][x+half]; cnt++; }
            if(y-half >= 0)    { sum += H[y-half][x]; cnt++; }
            if(y+half <= size) { sum += H[y+half][x]; cnt++; }
            float avg = (cnt? sum/cnt : 0.0f);
            H[y][x] = avg + frand(-scale, scale);
            UPDATED[y][x] = 1;
        }
    }
}

static void next_substep(){
    if(step_len < 2) return;
    clear_updated();

    if(phase == 0){
        diamond_step(step_len, jitter);
        phase = 1;
    } else {
        square_step(step_len, jitter);
        phase = 0;
        step_len /= 2;
        jitter   *= roughness;
        iter_level++;
    }
    glutPostRedisplay();
}

static void setColor(float h_norm){
    if(h_norm < 0.30f) glColor3f(0.05f,0.10f,0.65f);
    else if(h_norm < 0.52f) glColor3f(0.10f,0.55f,0.18f);
    else if(h_norm < 0.78f) glColor3f(0.45f,0.42f,0.38f);
    else glColor3f(0.95f,0.97f,0.98f);
}

static void drawTerrain(){
    float mn,mx; minmax(&mn,&mx);
    float inv = (mx>mn)? 1.0f/(mx-mn) : 1.0f;
    float off = size*0.5f;

    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);

    for(int y=0;y<size;y++){
        glBegin(GL_TRIANGLE_STRIP);
        for(int x=0;x<=size;x++){
            float h0 = (H[y][x]-mn)*inv;
            float h1 = (H[y+1][x]-mn)*inv;

            setColor(h0); glVertex3f(x-off, y-off,   H[y][x]*ZSCALE);
            setColor(h1); glVertex3f(x-off, (y+1)-off, H[y+1][x]*ZSCALE);
        }
        glEnd();
    }

    glPointSize(4.0f);
    glBegin(GL_POINTS);
    glColor3f(1.0f,0.15f,0.15f);
    for(int y=0;y<=size;y++){
        for(int x=0;x<=size;x++){
            if(UPDATED[y][x]){
                glVertex3f(x-off, y-off, H[y][x]*ZSCALE + 0.5f);
            }
        }
    }
    glEnd();
}

static void drawHUD(){
    char buf[256];
    sprintf(buf, "[N] step  [A] auto:%s  [R] reset  roughness=%.2f  level=%d  phase=%s  step=%d",
            autoplay?"on":"off", roughness, iter_level, (phase==0?"DIAMOND":"SQUARE"), step_len);

    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(0,1,0,1,-1,1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glDisable(GL_DEPTH_TEST);

    glColor3f(1,1,1);
    glRasterPos2f(0.02f, 0.02f);
    for(char* p=buf; *p; ++p) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *p);

    glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
}

static void display(){
    glClearColor(0.55f,0.75f,0.95f,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (double)glutGet(GLUT_WINDOW_WIDTH)/glutGet(GLUT_WINDOW_HEIGHT), 0.1, 5000.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(camX,camY,camZ,  0,0,0,  0,1,0);
    glRotatef(rotX,1,0,0);   // nuova rotazione verticale
    glRotatef(rotY,0,1,0);   // rotazione orizzontale

    drawTerrain();
    drawHUD();

    glutSwapBuffers();
}

static void reshape(int w,int h){ glViewport(0,0,w,h); }

static void timer(int){
    if(autoplay){
        next_substep();
        if(step_len < 2) autoplay = 0;
    }
    glutTimerFunc(120, timer, 0);
}

static void keyboard(unsigned char k,int x,int y){
    switch(k){
        case 27: exit(0);
        case 'n': case 'N': next_substep(); break;
        case 'a': case 'A': autoplay = !autoplay; break;
        case 'r': case 'R': reset_heightmap(); next_substep(); break;
        case '[': roughness = fmaxf(0.10f, roughness-0.05f); break;
        case ']': roughness = fminf(0.95f, roughness+0.05f); break;
        case 'w': case 'W': camZ -= 10.0f; break;  // avvicina
        case 's': case 'S': camZ += 10.0f; break;  // allontana
    }
}

static void special(int key,int x,int y){
    if(key==GLUT_KEY_LEFT)  rotY -= 4.0f;
    if(key==GLUT_KEY_RIGHT) rotY += 4.0f;
    if(key==GLUT_KEY_UP)    rotX -= 4.0f;
    if(key==GLUT_KEY_DOWN)  rotX += 4.0f;
}

int main(int argc,char**argv){
    srand((unsigned)time(NULL));
    reset_heightmap();

    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1100,800);
    glutCreateWindow("Diamond–Square 2D – step-by-step");

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutTimerFunc(120, timer, 0);
    glutIdleFunc(display);

    next_substep(); // subito primo DIAMOND
    glutMainLoop();
    return 0;
}
