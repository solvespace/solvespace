//-----------------------------------------------------------------------------
// The Cocoa-based implementation of platform-dependent GUI functionality.
//
// Copyright 2018 whitequark
//-----------------------------------------------------------------------------
#import  <AppKit/AppKit.h>
#include "solvespace.h"

//-----------------------------------------------------------------------------
// Objective-C bridging
//-----------------------------------------------------------------------------

@interface SSFunction : NSObject
- (SSFunction *)initWithFunction:(std::function<void ()> *)aFunc;
- (void)run;
@end

@implementation SSFunction
{
    std::function<void ()> *func;
}

- (SSFunction *)initWithFunction:(std::function<void ()> *)aFunc {
    if(self = [super init]) {
        func = aFunc;
    }
    return self;
}

- (void)run {
    if(*func) (*func)();
}
@end

namespace SolveSpace {
namespace Platform {

//-----------------------------------------------------------------------------
// Timers
//-----------------------------------------------------------------------------

class TimerImplCocoa : public Timer {
public:
    NSTimer *timer;

    TimerImplCocoa() : timer(NULL) {}

    void WindUp(unsigned milliseconds) override {
        SSFunction *callback = [[SSFunction alloc] init:&this->onTimeout];
        NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:
            [callback methodSignatureForSelector:@selector(run)]];
        [invocation setTarget:callback];
        [invocation setSelector:@selector(run)];

        if(timer != NULL) {
            [timer invalidate];
        }
        timer = [NSTimer scheduledTimerWithTimeInterval:(milliseconds / 1000.0)
            invocation:invocation repeats:NO];
    }

    ~TimerImplCocoa() {
        if(timer != NULL) {
            [timer invalidate];
        }
    }
};

TimerRef CreateTimer() {
    return std::unique_ptr<TimerImplCocoa>(new TimerImplCocoa);
}

}
}
