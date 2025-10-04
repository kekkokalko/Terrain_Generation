#include <stdlib.h>          // rand, srand
#include <math.h>            // floorf, tanhf, cosf, sinf
#include <GL/glut.h>         // OpenGL/GLUT

// --- Dimensioni della mesh (terreno) e della griglia (lattice di Perlin) ---
#define MAP_W 120            // colonne della mesh
#define MAP_H 120            // righe della mesh
#define G_W   64             // nodi X del lattice
#define G_H   64             // nodi Y del lattice

// --- Dati principali ---
static float H[MAP_H][MAP_W];          // heightmap finale
static float GX[G_H][G_W], GY[G_H][G_W]; // gradienti ai nodi del lattice

// --- Parametri del rumore ---
static float s=0.08f;        // scala spaziale (frequenza base)
static int   oct=7;          // numero di ottave fBm
static float gain=0.5f;      // attenuazione ampiezza per ottava
static float lac=2.0f;       // moltiplicatore di frequenza per ottava

// --- Camera ---
static float ax=-35;         // rotazione intorno a Y
static float ay= 35;         // rotazione intorno a X
static float dz=120;         // distanza dalla scena

// --- Flag per mostrare la griglia ---
static int showGrid=1;

// ----------------- Funzioni Perlin -----------------

// funzione fade di Perlin (smussa le interpolazioni)
static inline float fade(float t){ return t*t*t*(t*(t*6-15)+10); }

// interpolazione lineare
static inline float lerp(float a,float b,float t){ return a+t*(b-a); }

// prodotto scalare tra gradiente e offset locale
static inline float gdot(int i,int j,float dx,float dy){
  i=(i%G_W+G_W)%G_W;                 // wrap X
  j=(j%G_H+G_H)%G_H;                 // wrap Y
  return GX[j][i]*dx+GY[j][i]*dy;    // grad · offset
}

// calcolo del Perlin noise 2D in un punto
static float perlin2(float x,float y){
  int i0=floorf(x), j0=floorf(y);    // nodo basso/sinistro
  int i1=i0+1, j1=j0+1;              // nodo alto/destro
  float tx=x-i0, ty=y-j0;            // offset frazionari
  float u=fade(tx), v=fade(ty);      // fade
  float d00=gdot(i0,j0,tx,ty);       // contributo (0,0)
  float d10=gdot(i1,j0,tx-1,ty);     // contributo (1,0)
  float d01=gdot(i0,j1,tx,ty-1);     // contributo (0,1)
  float d11=gdot(i1,j1,tx-1,ty-1);   // contributo (1,1)
  float nx0=lerp(d00,d10,u);         // interp X riga bassa
  float nx1=lerp(d01,d11,u);         // interp X riga alta
  return lerp(nx0,nx1,v);            // interp finale su Y
}

// fBm: somma di più ottave di Perlin
static float fbm2(float x,float y){
  float sum=0, a=1, f=1;
  for(int o=0;o<oct;o++){            // per ogni ottava
    sum+=a*perlin2(x*f,y*f);         // somma contributo
    a*=gain;                         // riduci ampiezza
    f*=lac;                          // aumenta frequenza
  }
  return sum;
}

// ----------------- Generazione dati -----------------

// costruisce i gradienti del lattice
static void buildGrad(unsigned seed){
  srand(seed);
  for(int j=0;j<G_H;j++){
    for(int i=0;i<G_W;i++){
      float a=(rand()/(float)RAND_MAX)*6.2831853f; // angolo casuale
      GX[j][i]=cosf(a);                            // componente X
      GY[j][i]=sinf(a);                            // componente Y
    }
  }
}

// costruisce la heightmap da fBm
static void buildHeight(void){
  for(int j=0;j<MAP_H;j++){
    for(int i=0;i<MAP_W;i++){
      float h=fbm2(i*s,j*s);       // valore fBm
      h=tanhf(0.6f*h);             // smorzamento
      H[j][i]=h*18.0f;             // scala in altezza
    }
  }
}

// ----------------- Rendering -----------------

// colore in base alla quota
static void colorH(float h){
  if(h<-6) glColor3f(0,0.2,0.7);
  else if(h<-1) glColor3f(0.1,0.5,0.8);
  else if(h<4)  glColor3f(0.1,0.6,0.2);
  else if(h<10) glColor3f(0.4,0.3,0.2);
  else          glColor3f(0.9,0.9,0.9);
}

// disegna la griglia del lattice sotto il terreno
static void drawGrid(void){
  if(!showGrid) return;
  float X0=-(MAP_W*0.5f), Z0=-(MAP_H*0.5f), Y=-12.0f;
  float sx=(float)MAP_W/(G_W-1), sz=(float)MAP_H/(G_H-1);

  glDisable(GL_DEPTH_TEST);         // disegno indipendente dal depth
  glEnable(GL_BLEND);               // attiva trasparenza
  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

  glLineWidth(1);
  glColor4f(0.1,0.1,0.1,0.75);
  glBegin(GL_LINES);
  for(int j=0;j<G_H;j++){           // linee orizzontali
    float z=Z0+j*sz;
    glVertex3f(X0,Y,z); glVertex3f(X0+MAP_W,Y,z);
  }
  for(int i=0;i<G_W;i++){           // linee verticali
    float x=X0+i*sx;
    glVertex3f(x,Y,Z0); glVertex3f(x,Y,Z0+MAP_H);
  }
  glEnd();

  glPointSize(3);
  glColor4f(0.05,0.05,0.05,0.85);
  glBegin(GL_POINTS);               // nodi come punti
  for(int j=0;j<G_H;j++){
    float z=Z0+j*sz;
    for(int i=0;i<G_W;i++){ float x=X0+i*sx; glVertex3f(x,Y,z); }
  }
  glEnd();

  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
}

// disegna il terreno
static void drawTerrain(void){
  glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
  glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
  for(int j=0;j<MAP_H-1;j++){
    glBegin(GL_TRIANGLE_STRIP);
    for(int i=0;i<MAP_W;i++){
      float h1=H[j+1][i], h0=H[j][i];
      colorH(h1); glVertex3f(i-(MAP_W*0.5f),h1,(j+1)-(MAP_H*0.5f));
      colorH(h0); glVertex3f(i-(MAP_W*0.5f),h0,j-(MAP_H*0.5f));
    }
    glEnd();
  }
  glDisable(GL_CULL_FACE);
}

// ----------------- Callback GLUT -----------------

// display
static void display(void){
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glMatrixMode(GL_MODELVIEW); glLoadIdentity();
  glTranslatef(0,-10,-dz);       // sposta camera
  glRotatef(ay,1,0,0);           // inclina
  glRotatef(ax,0,1,0);           // ruota
  drawTerrain();
  drawGrid();
  glutSwapBuffers();
}

// reshape
static void reshape(int w,int h){
  if(h==0) h=1;
  glViewport(0,0,w,h);
  glMatrixMode(GL_PROJECTION); glLoadIdentity();
  gluPerspective(60.0,(float)w/h,0.1,1000);
}

// tastiera
static void keyboard(unsigned char k,int x,int y){
  if(k==27) exit(0);             // ESC
  if(k=='a') ax-=5; if(k=='d') ax+=5;
  if(k=='w') ay+=5; if(k=='s') ay-=5;
  if(k=='+') dz-=5; if(k=='-') dz+=5;
  if(k=='g'||k=='G') showGrid=!showGrid;
  glutPostRedisplay();
}

// ----------------- Init + main -----------------

static void init(void){
  glEnable(GL_DEPTH_TEST);
  glClearColor(0.6,0.8,1.0,1.0);
  buildGrad(12345u);
  buildHeight();
}

int main(int argc,char** argv){
  glutInit(&argc,argv);
  glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB|GLUT_DEPTH);
  glutInitWindowSize(900,700);
  glutCreateWindow("Perlin Landscape + Lattice Grid");
  init();
  glutDisplayFunc(display);
  glutReshapeFunc(reshape);
  glutKeyboardFunc(keyboard);
  glutMainLoop();
  return 0;
}
