# TODO
- collect persons on the way
	- tests: collect, pickup
- implement correct usage of queue
	- removeFromQueue (when last person going to queued floor leaves elevator)
	- check where to go after starting movement using queue
- implement correct behavior when beeping
	- void HandleBeeping(Environment &env, const Event &e);
	- void HandleBeeped(Environment &env, const Event &e);
- check weight limitss