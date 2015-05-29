notify:
	from outside:
		for each elevator that stops here:
			space left:
				moving:
					at the same floor:
						on the way to middle:
							stop at middle. (TODO)
						other direction:
							next. (NOTICE, should not happen)
					on the way:
						add current floor to queue.
					other direction:
						next.
				idle:
					already there:
						open doors.
					somewhere else:
						go to caller.
			no space left:
				next.
		try again later.
	from inside:
		add target floor to queue.

process queue:
	queue empty:
		do nothing.
	queue not empty:
		nothing in the way:
			at lowest:
				go up to next.
			at highest:
				go down to next.
			else:
		something in the way:
			try again later.
