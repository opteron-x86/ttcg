// Captions for rule demo frames that carry none of their own.
function lessonCardName(d){return state.cards[d.hands[0][d.hand]]?.name||'the highlighted card';}
function lessonRank(d,square,side){return state.cards[d.board[square]?.card]?.sides[side];}
function basicLessonPrompt(d){
 const rank=(q,s)=>lessonRank(d,q,s),choose=`Choose ${lessonCardName(d)}.`;
 return [
  choose,
  'Take turns filling the board. Watch Opponent place a card.',
  `Diagonal cards do not capture each other. ${choose}`,
  `${rank(3,1)} beats ${rank(4,3)}. The captured card turns blue and counts for you.`,
  `${rank(1,2)} against ${rank(4,0)}: equal numbers do not capture. ${choose}`,
  'Only touching sides are compared. Watch Opponent’s next move.',
  `${rank(5,0)} beats ${rank(2,2)}, and ${rank(5,3)} beats ${rank(4,1)}. One placement can capture two cards. ${choose}`,
  `Your ${rank(8,0)} cannot beat their ${rank(5,2)}. Strong sides help defend a card.`,
  `Opponent captures with ${rank(6,0)} against ${rank(3,2)}. ${choose}`,
  'Five each—a draw. The unplayed card counts too. More than five wins.'
 ][d.step]||'';
}
function comboLessonPrompt(d,plus){
 if(!d.step)return `${plus?'Make two touching pairs add to the same total.':'Match two touching sides to capture with Same.'} Choose ${lessonCardName(d)}.`;
 const a=lessonRank(d,0,1),b=lessonRank(d,1,3),c=lessonRank(d,0,2),e=lessonRank(d,3,0);
 return (plus?`${a} + ${b} and ${c} + ${e} both make ${a+b}. Plus`:`${a} matches ${b}, and ${c} matches ${e}. Same`)+' captures both cards, then a captured card takes its neighbour for Combo.';
}
function lessonPrompt(d,selected=false){
 if(selected)return 'Place it in the highlighted square.';
 if(d.caption)return d.caption;
 if(d.workshopRule)return workshopPrompt(d,selected);
 if(d.plusLesson)return comboLessonPrompt(d,true);
 if(d.sameLesson)return comboLessonPrompt(d,false);
 return basicLessonPrompt(d);
}
function workshopPrompt(d,selected=false){
 if(d.workshopRule===4){if(selected)return 'Place it in the highlighted square.';return ['Place Barded Guar. Under Reverse, lower touching numbers win.','Watch the opponent play against the Guar.','The lower touching rank captures the Guar. Equal ranks still do not capture.'][d.step];}

 if(d.riftTour){if(selected)return 'Place it in the highlighted square.';
  return ["Three opposing cards are shown after the deal. Two stay hidden. Choose the highlighted card.","Watch the opponent’s placement. A hidden card is revealed when played.","Your opponent also sees only three of your starting cards. Choose the highlighted card.","Keep track of which cards have been played.","We’ll set up two touching pairs for Plus. Choose the highlighted card.","Watch the opponent complete the setup.","Your opponent has not seen this card. Place Mistveil Enchanter in the corner.","The matching sums trigger Plus. The captured cards can then take their neighbours for Combo.","One square remains. Choose your last card.","Under Direct trade, you keep the cards you control—even in a draw. This lesson uses borrowed cards."][d.step];
 }

 if(d.workshopRule===16){if(d.step===0)return 'Open reveals both hands. Choose Piercing Twilight.';return selected?'Place it in the highlighted square.':basicLessonPrompt(d);}
 if(d.affinityTour){if(selected)return "Place it in the highlighted square.";return ["Place Barded Guar on the Endurance square.","Its matching affinity gives every side +1. Watch the next placement.","The attacking 3 ties the Guar’s boosted 3. Without Affinity it would capture. Place the neutral card on Agility.","Neutral cards have no matching affinity: every side loses 1. Watch the opponent’s Guar.","Their Guar also loses 1 on Agility. Place your next card on the marked square.","A matching affinity gives +1. Blank squares leave every card unchanged."][d.step];}
 if(d.workshopRule===64){if(d.step===0)return 'Three opposing cards are visible. The other two stay hidden until played. Choose Piercing Twilight.';return selected?'Place it in the highlighted square.':basicLessonPrompt(d);}
 if(selected)return 'Place it in the highlighted square.';
 if(d.step===0)return d.workshopRule===32?'Place Barded Guar on the Endurance square.':'Place Barded Guar to bring Beast to three cards.';
 if(d.step===1)return d.workshopRule===32?'A matching affinity gives +1 to every side. Watch the next placement.':d.workshopRule===128?'Beast reached three. All Beast cards gain +1, on both sides. Watch the next placement.':'Beast reached three. All Beast cards lose 1, on both sides. Watch the next placement.';
 return d.workshopRule===256?'The Scamp’s 2 beats the Guar’s reduced 1. Beast keeps −1 for the rest of this match.':d.workshopRule===128?'The Guar’s raised 3 defends against 2. Beast keeps +1 for the rest of this match.':'The Guar’s matching affinity raises 2 to 3, tying the attacking 3 and preventing capture. A mismatch gives −1; blank squares change nothing.';
}
