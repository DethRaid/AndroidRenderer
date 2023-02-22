# Render Backend

SAH has a render backend. This is meant to abstract away the low level GPu concerns. It handles 
things like memory and resource allocation, performs basic resource state tracking, and manages
renderpasses and subpasses. It's designed to be easy to use while efficiently utilizing TBDR 
hardware
