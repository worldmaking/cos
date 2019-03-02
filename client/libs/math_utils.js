function quat_uy(out, q) {
	out[0] = (2.) * ((q[0] * q[1]) - (q[3] * q[2]));
	out[1] = (1.) - 2. * ((q[0] * q[0]) + (q[2] * q[2]));
	out[2] = (2.) * ((q[1] * q[2]) + (q[3] * q[0]));
	return out;
}