# TODO
- pass tests:
	-2lazy (prioritize queue by waiting time)
	-2speed (prioiritize elevator call by speed and distance)
- simplify prioritizing elevator calls
- check where to go next in queue
- implement correct behavior when beeping
	- void HandleBeeping(Environment &env, const Event &e);
	- void HandleBeeped(Environment &env, const Event &e);
- check weight limits

# MAYBE
- queue as ordered set with comparison by IsBelow/IsAbove
- split queue up in all floors below elevator and all above
- prioritize queue by waiting persons vs. persons in elevator