/* datafile.cc
**
**	Implements data file routines.
*/

#include <mdefs.h>
#include <fstream.h>
#include <game/font.h>
#include "gjid.h"

extern char * story;
extern WORD StorySize;

void LoadData (char * filename)
{
Level * NewLevel;
ifstream is;
int i;

    is.open (filename); 

    cout << "Reading palette.\n";
    // Special palette is used
    pal.Read (is);
    
    cout << "Reading font.\n";
    // Font
    font.Read (is);

    cout << "Reading " << NumberOfPics << " pictures";
    // Read the pictures
    for (i = 0; i < NumberOfPics; ++ i) {
       pics[i].Read (is);
       cout << ".";
    }
    cout << "\n";

    // Read the story
    cout << "Reading story.\n";
    is.read (&StorySize, sizeof(WORD));
    story = new char [StorySize];
    is.read (story, StorySize);

    // Read the levels
    is.read (&nLevels, sizeof(WORD));
    if (nLevels == 0)
       cout << "No levels found in data file!";
    else if (nLevels > MAX_LEVELS) {
       cout << "Too many levels! (" << nLevels;
       cout << ", maximum is " << MAX_LEVELS << ")\n";
       exit (1);
    }
    else
       cout << "Reading " << nLevels << " levels";

    for (i = 0; i < nLevels; ++ i) {
       NewLevel = new Level;
       NewLevel->Read (is);
       levels.Tail();
       levels.InsertAfter (NewLevel);
       cout << ".";
    }
    cout << "\n";

    is.close();
}

void SaveData (char * filename)
{
ofstream os;
int i;

    os.open (filename); 

    pal.Write (os);
    font.Write (os);

    // Write the pictures
    for (i = 0; i < NumberOfPics; ++ i)
       pics[i].Write (os);

    // Read the levels
    os.write (&nLevels, sizeof(WORD));
    levels.Head (1);
    for (i = 0; i < nLevels; ++ i) {
       levels.LookAt (1)->Write (os);
       levels.Next (1);
    }

    os.close();
}

