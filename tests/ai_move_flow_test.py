"""Run the production AI-move handoff against a small table host."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/MatchFlow.cpp').read_text()
begin = source.index('void applyAIMove(')
handoff = source[begin:source.index('void runAI() {', begin)]
code = r"""
#include <atomic>
#include <cassert>
#include <cstdint>
namespace host {
namespace ttcg {
struct Move {int hand=-1,square=-1,group=-1;};
struct Result {bool legal=true;};
struct Match {
 int turn=1,plays=0;bool legal=true;
 Result play(Move){++plays;if(legal)turn=0;return {legal};}
};
}
namespace log {template<class... A>void error(A&&...){}}
std::atomic<std::uint64_t> epoch{5};
bool visible=true,thinking=true,live=true;
std::uint64_t session=3,revision=7;
ttcg::Match match;
int searches=0,published=0,recorded=0,closed=0;
bool checkLiveWorld(){if(!live)thinking=false;return live;}
void runAI(){++searches;thinking=true;}
void close(){++closed;thinking=false;}
void recordResult(const ttcg::Result&){++recorded;}
void publish(){++published;}
HANDOFF
void reset(){
 epoch=5;visible=true;thinking=true;live=true;session=3;revision=7;match={};
 searches=published=recorded=closed=0;
}
}
using namespace host;
int main(){
 // The table is as the search left it: the move is played once.
 reset();applyAIMove({},5,3,7);
 assert(match.plays==1&&recorded==1&&published==1&&!searches&&!thinking);
 // Something was published during the search. The opponent must not stay
 // "thinking" for ever: it lets go of the stale move and searches again.
 reset();revision=8;applyAIMove({},5,3,7);
 assert(!match.plays&&!recorded&&searches==1&&thinking);
 reset();match.turn=0;applyAIMove({},5,3,7);
 assert(!match.plays&&searches==1);
 // A closed or replaced table is left alone.
 reset();epoch=6;thinking=false;applyAIMove({},5,3,7);
 assert(!match.plays&&!searches&&!thinking);
 reset();session=4;thinking=false;applyAIMove({},5,3,7);
 assert(!match.plays&&!searches);
 reset();visible=false;thinking=false;applyAIMove({},5,3,7);
 assert(!match.plays&&!searches);
 // The world interrupted the game: the interruption already released the table.
 reset();live=false;applyAIMove({},5,3,7);
 assert(!match.plays&&!searches&&!thinking);
 // An illegal move closes the table instead of freezing it.
 reset();match.legal=false;applyAIMove({},5,3,7);
 assert(match.plays==1&&closed==1&&!recorded&&!thinking);
}
""".replace('HANDOFF', handoff)
with tempfile.TemporaryDirectory(prefix='tessera-ai-move-') as directory:
    cpp = Path(directory) / 'handoff.cpp'
    exe = Path(directory) / 'handoff'
    cpp.write_text(code)
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('AI move handoff: stale searches run again, closed tables are left alone, illegal moves close the table')
