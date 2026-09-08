# Evaluation harness

Harness code lives here when needed. It may read agent metadata, launch agent entrypoints, provide contract input, capture output, record timing/usage, and normalize failures.

Dependency direction is one-way: the harness may know about agents; agents must not import or reference the harness.
