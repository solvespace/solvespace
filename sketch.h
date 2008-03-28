
#ifndef __SKETCH_H
#define __SKETCH_H

class hEntity;
class hPoint;
class hRequest;
class hParam;

class hRequest {
public:
    int     v;

    hEntity entity(int i);
};

class Request {
public:
    static const int REQUEST_LINE_SEGMENT                       = 0;
    static const int REQUEST_STEP_REPEAT_TRANSLATE              = 1;
    static const int REQUEST_STEP_REPEAT_TRANSLATE_SYM          = 2;
    int         type;

    hRequest    h;
};

class hEntity {
public:
    int     v;

    hRequest request(int i);
    hPoint point(int i);
};

class Entity {
public:
    static const int ENTITY_LINE_SEGMENT                        = 0;
    static const int ENTITY_PWL_SEGMENT                         = 1;
    int         type;

    hEntity     h;
};

class hPoint {
public:
    int     v;
};

class Point {
public:
    
    hPoint      h;
};

class hParam {
public:
    int         v;
};

class Param {
public:
    double      val;

    hParam      h;
};

#endif

