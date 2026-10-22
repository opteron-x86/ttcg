#include "Lesson.h"
#include <fstream>
#include <iostream>
int main(int argc,char** argv){
 if(argc<2){std::cerr<<"usage: rule_demo_host out.js\n";return 2;}
 std::ofstream out(argv[1]);
 if(!out)return 1;
 out<<"const ruleDemos="<<ttcg::allRuleDemosJson()<<";\n";
 out<<"const guidedDemos={\"rift\":"<<ttcg::Lesson::riftLesson().framesJson()<<",\"affinityWorkshop\":"<<ttcg::Lesson::workshop(ttcg::Affinity).framesJson()<<"};\n";
 return 0;
}
