Implement double-click support.

SDL2/3 just emit mouse-down/up..  we need to implement a deferred wait for fireing the click event to see if this
was a double-click event or a single-click event...

There is a timer class (src/Core/Timer.h) which perhaps can be used for this or enhanced for this purpose.
It is mainly used to sample durations - but in principle it could be used to handle dbl-click deferral..

Basically - if a down/up mouse event occurs we create a timer with the configurable 
item (dbl_click_speed) - in milliseconds - if another down occurs before the timer has passed we stop the timer
and process it as a dbl-click instead...  If the timer elapses - it fires as a single click...

I am open for other suggestions - I have never really implemented a dbl-click for raw mouse events before.. 
making a few assumptions here on how it can be achieved...
