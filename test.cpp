#include <iostream>

using namespace std;

extern "C" {
	double f(double x);
}

int main() {
	cout << f(1.0) << endl;
	return 0;
}
