# TODO
- check where to go next in queue
- implement correct behavior when beeping
	- void HandleBeeping(Environment &env, const Event &e);
	- void HandleBeeped(Environment &env, const Event &e);
- check weight limits

# MAYBE
- queue as ordered set with comparison by IsBelow/IsAbove
- split queue up in all floors below elevator and all above
- prioritize queue by waiting persons vs. persons in elevator