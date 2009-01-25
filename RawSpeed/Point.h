#ifndef SS_Point_H
#define SS_Point_H


class iPoint2D {
public:
	iPoint2D() {x = y = 0;  }
	iPoint2D( int a, int b) {
		x=a; y=b;}
	~iPoint2D() {};
  int x,y;
};

#endif // SS_Point_H
