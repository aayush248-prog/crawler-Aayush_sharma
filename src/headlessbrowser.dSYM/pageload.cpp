#include<iostream>
#include<thread>
#include<fstream>
#include<string>
#include<stdexcept>
#include<unistd.h>

using namespace std;
class pageload{

public:

    Browser browser;

    int pagecount;


    pageload()
    {
        pagecount=0;
    }

    getpage(string path){
    ifstream file("pages/page1.txt");

    if(!file.is_open())
    {
        cout<<"File not found";
        return 0;
    }

    string text;

    while(getline(file,text))
    {
        cout<<text<<endl;
    }

    file.close();

    }
    void loadpage(string url)
    {
        try
        {
            if(!browser.getSocketURL())
            {
                throw runtime_error("Browser socket unavailable");
            }

            browser.openPage(url);

        }
        catch(exception &err)
        {
            cout<<err.what()<<endl;
        }
    }


    string savepage(string name)
    {
        try
        {
            string path="./include/pages/" + name + ".txt";

            ofstream file(path);

            if(!file.is_open())
            {
                throw runtime_error("File cannot open");
            }


            pagecount++;

            string text=browser.getHtml();

            file << text;

            file.close();

            return path;
        }
        catch(exception &err)
        {
            cout<<err.what()<<endl;
            return "";
        }
    }


    int pages()
    {
        return pagecount;
    }

};