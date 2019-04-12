#include <iostream>

using namespace std;

int main()
{
    int start = 12;
    int mesh_row = 4;
    bool sense_inc = true;
    for (int i = 0; i < 100; i++) {
        if (start == mesh_row*mesh_row - mesh_row)
            sense_inc = false;
        if (start == 0)
            sense_inc = true;
        /* code */
        cout << "next start: " << start  << endl;
        if (sense_inc) {

            if ((start/mesh_row) % 2 == 0) {
                // this means this is an even mesh_row
                // starting from 0.
                if (start%mesh_row == mesh_row-1) {

                    start = start + mesh_row;
                }
                else
                    start++;
            }
            else if ((start/mesh_row) % 2 == 1) {
                // this means this is an odd mesh_row
                // starting from 0.
                if (start%mesh_row == 0) {
                    // cout << "next start: " << start << endl;
                    start = start + mesh_row;
                }
                else
                    start--;
            }
        }
        else if (!sense_inc) {

            if ((start/mesh_row) % 2 == 0) {
                // this means this is an even mesh_row
                // starting from 0.
                if (start%mesh_row == 0) {

                    start = start - mesh_row;
                }
                else
                    start--;
            }
            else if ((start/mesh_row) % 2 == 1) {
                // this means this is an odd mesh_row
                // starting from 0.
                if (start%mesh_row == mesh_row-1) {
                    // cout << "next start: " << start << endl;
                    start = start - mesh_row;
                }
                else
                    start++;
            }
        }
    }
    return 0;
}
