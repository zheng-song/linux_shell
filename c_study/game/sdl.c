#include <stdlib.h>
#include <SDL/SDL.h>
#include <SDL/SDL_draw.h>
#include <math.h>

int showRainDrop(SDL_Surface *screen);
int drawCycle(SDL_Surface *screen);
int drawRectangle(SDL_Surface *screen);

int main(int argc, char const *argv[])
{
	SDL_Surface *screen;
	
	Uint32 color;

	if(SDL_Init(SDL_INIT_VIDEO) == -1){
		return -1;
	}

	screen = SDL_SetVideoMode(640,480,16,SDL_SWSURFACE);
	if(screen == NULL){
		return -1;
	}

	atexit(SDL_Quit);



	color = SDL_MapRGB(screen->format,255,0,0);
	SDL_FillRect(screen,NULL,color);
	SDL_UpdateRect(screen,0,0,0,0);
	SDL_Delay(50);

	color = SDL_MapRGB(screen->format,0,255,0);

	int i;
	int fix = 240;
	for ( i = 0; i < 640; ++i)
	{
		double x = (double)i;
		double y =  50*sin(x/180*M_PI);
		printf("%f\n",y );
		int j = fix + (int) y;
		Draw_Line(screen,i,j,i,j,color);
	}
	SDL_UpdateRect(screen,0,0,0,0);
	SDL_Delay(1000);


	showRainDrop(screen);

	drawCycle(screen);
	
	drawRectangle(screen);
	return 0;
}




int drawRectangle(SDL_Surface *screen)
{
	SDL_FillRect(screen,NULL,SDL_MapRGB(screen->format,0,0,0));
	SDL_UpdateRect(screen,0,0,0,0);
	Draw_Rect(screen,80,180,160,120,SDL_MapRGB(screen->format,255,255,255));
	Draw_Rect(screen,319,179,240,122,SDL_MapRGB(screen->format,255,255,0));
	Draw_FillRect(screen,320,180,240,120,SDL_MapRGB(screen->format,255,0,0));
	SDL_UpdateRect(screen,0,0,0,0);
	SDL_Delay(3000);
	return 0;
}

int drawCycle(SDL_Surface *screen)
{
	Uint32 color; 
	color = SDL_MapRGB(screen->format,255,255,255);
	SDL_FillRect(screen,NULL,color);
	SDL_UpdateRect(screen,0,0,0,0);

	color = SDL_MapRGB(screen->format,0,0,0);
	for (int r = 5; r <= 65; r+=15){
		Draw_Circle(screen,320,240,r,color);
		SDL_UpdateRect(screen,0,0,0,0);
		SDL_Delay(500);
	}

	SDL_Delay(2000);
	return 0;
}



int showRainDrop(SDL_Surface *screen)
{
	SDL_FillRect(screen,NULL,SDL_MapRGB(screen->format,0,0,0));
	SDL_UpdateRect(screen,0,0,0,0);
	int x,y,y1,y2;
	int c[3],rx[3],ry[3];
	int i;
for(int num = 0; num < 1; num++){
	printf("num is :%d\n",num);
	srand((unsigned)time(NULL));
	i = rand();
	x = (int)(120+400.0*rand()/RAND_MAX); 
	y = (int)(240+180.0*rand()/RAND_MAX); 
	y1 = 0;
	y2 = 0;

	while(y1<=y){
		Draw_Line(screen,x,y1,x,0,SDL_MapRGB(screen->format,255,255,255));
		Draw_Line(screen,x,y2,x,0,SDL_MapRGB(screen->format,0,0,0));
		SDL_UpdateRect(screen,0,0,0,0);
		SDL_Delay(10);
		y1+=10;
		if (y1>=50)
			y2=y1-50;
	}
	for (i = 0; i < 3; ++i){
		c[i] = 255;
		rx[i] = 6;
		ry[i] = 3;
	}
	while(c[2] != 0){
		for (i = 0; i < 3; ++i){
			if (i!= 0 && c[0] > 255-i*50)continue;
			if(c[i] != 0){
				Draw_Ellipse(screen,x,y,rx[i],ry[i],SDL_MapRGB(screen->format,c[i],c[i],c[i]));
				SDL_Delay(20);
			}
		}
		SDL_UpdateRect(screen,0,0,0,0);
		for (i = 0; i < 3; ++i){
			if(i!=0 && c[0]>255-i*50)continue;
			if(c[i]!=0){
				Draw_Ellipse(screen,x,y,rx[i],ry[i],SDL_MapRGB(screen->format,0,0,0));
				c[i]-=5;
				rx[i]+=2;
				ry[i]+=1;
			}
		}
		y2+=10;
		Draw_Line(screen,x,y2,x,0,SDL_MapRGB(screen->format,0,0,0));
	}
	SDL_Delay(1000);
}
	return 0;

}