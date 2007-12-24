#include "character.h"
#include "util/bitmap.h"
#include "util/load_exception.h"
#include "nameplacer.h"
#include "animation.h"
#include "util/font.h"
#include "world.h"
#include "globals.h"
#include "util/funcs.h"
#include "buddy_player.h"
#include "factory/font_render.h"

static const char * PLAYER_FONT = "/fonts/arial.ttf";

static const int SPAWN_TIME = 1500;

BuddyPlayer::BuddyPlayer( const Character * leader, const Character & chr ) throw( LoadException ):
Character( chr ),
name_id(-1),
invincible( false ),
want_x( -1 ),
want_z( -1 ),
spawn_time( SPAWN_TIME ),
leader( leader ){
	show_life = getHealth();
	int x, y;
	NamePlacer::getPlacement( x, y, name_id );
}
	
void BuddyPlayer::draw( Bitmap * work, int rel_x ){
	Character::draw( work, rel_x );

	int x1, y1;
	NamePlacer::getPlacement( x1, y1, name_id );
	
	if ( icon )
		icon->draw( x1, y1, *work );

	int hasIcon = icon ? icon->getWidth() : 0;
	

	if ( show_life < 0 ){
		show_life = 0;
	}

	const Font & player_font = Font::getFont( Util::getDataPath() + PLAYER_FONT, 20, 20 );
	const string & name = getName();
	int nameHeight = player_font.getHeight( name ) / 2;
	nameHeight = 20 / 2;
	FontRender * render = FontRender::getInstance();
	render->addMessage( player_font, (hasIcon + x1) * 2, y1 * 2, Bitmap::makeColor(255,255,255), -1, name );
	drawLifeBar( hasIcon + x1, y1 + nameHeight, work );
	// int max = getMaxHealth() < 100 ? getMaxHealth() : 100;
	// render->addMessage( player_font, (x1 + hasIcon + max + 5) * 2, y1 + nameHeight, Bitmap::makeColor(255,255,255), -1, "x %d", getLives() );

}
	
void BuddyPlayer::drawLifeBar( int x, int y, Bitmap * work ){
	drawLifeBar( x, y, show_life, work );
}
	
Object * BuddyPlayer::copy(){
	return new BuddyPlayer( *this );
}

/*
static int furthestFriend( vector< Object * > * others, int alliance, Object * me ){
	double x = -1;
	for ( vector< Object * >::iterator it = others->begin(); it != others->end(); it++ ){
		Object * o = *it;
		if ( o != me && o->getAlliance() == alliance && o->getX() > x ){
			x = o->getX();
		}
	}

	return (int) x;
}
*/

const Object * BuddyPlayer::findClosest( const vector< Object * > & enemies ){
	Object * e = NULL;
	double max = 0;
	for ( vector< Object * >::const_iterator it = enemies.begin(); it != enemies.end(); it++ ){
		Object * current = *it;
		/* should probably see if current is a character.. */
		double distance = fabs( current->getX() - getX() );
		if ( e == NULL || distance < max ){
			e = current;
			max = distance;
		}
	}

	return e;
}

void BuddyPlayer::act( vector< Object * > * others, World * world, vector< Object * > * add ){
	Character::act( others, world, add );

	if ( show_life > getHealth() ){
		show_life--;
	}

	if ( show_life < getHealth() ){
		show_life++;
	}

	vector< Object * > enemies;
	
	if ( getStatus() != Status_Ground && getStatus() != Status_Jumping )
		return;

	filterEnemies( enemies, others );
		
	if ( animation_current->Act() ){
		animation_current->reset();
		// nextTicket();
		// animation_current = movements[ "idle" ];
		animation_current = getMovement( "idle" );
		animation_current->reset();
	}

	if ( animation_current == getMovement( "idle" ) ||
	     animation_current == getMovement( "walk" ) ){
		if ( enemies.empty() && want_x == -1 && want_z == -1 && Util::rnd( 15 ) == 0 ){
			// want_x = Util::rnd( 100 ) - 50 + furthestFriend( others, getAlliance(), this );
			want_x = Util::rnd( 100 ) - 50 + (int) leader->getX();
			want_z = Util::rnd( world->getMinimumZ(), world->getMaximumZ() );
		} else if ( ! enemies.empty() ){
			const Object * main_enemy = findClosest( enemies );
			if ( main_enemy->getX() > getX() ){
				want_x = (int)(main_enemy->getX() - 20 - Util::rnd( 20 ));
			} else {
				want_x = (int)(main_enemy->getX() + 20 + Util::rnd( 20 ));
			}
			if ( want_x < 1 ){
				want_x = Util::rnd( 100 ) - 50 + (int) leader->getX();
			}
			want_z = (int)(Util::rnd( 3 ) - 1 + main_enemy->getZ());
			faceObject( main_enemy );

			if ( Util::rnd( 35 ) == 0 ){
				vector< Animation * > attacks;
				for ( map<string,Animation *>::const_iterator it = getMovements().begin(); it != getMovements().end(); it++ ){
					Animation * maybe = (*it).second;
					if ( maybe->isAttack() && maybe->getStatus() == Status_Ground && maybe->getName() != "special" )
						attacks.push_back( maybe );
				}

				double attack_range = fabs( getX() - main_enemy->getX() );
				double zdistance = ZDistance( main_enemy );
				for ( vector< Animation * >::iterator it = attacks.begin(); it != attacks.end(); ){
					Animation * maybe = *it;
					if ( attack_range > maybe->getRange() || zdistance > maybe->getMinZDistance() ){
						it = attacks.erase( it );
					} else {
						it++;
					}
				}

				if ( !attacks.empty() ){
					animation_current = attacks[ Util::rnd( attacks.size() ) ];
					world->addMessage( animationMessage() );
					nextTicket();
					animation_current->reset();
					return;
				} else {
				}
			}
		}

		if ( want_x != -1 && want_z != -1 ){
			bool walk = false;
			if ( getX() - want_x < -2 ){
				moveX( getSpeed() );
				setFacing( FACING_RIGHT );
				walk = true;
			} else if ( getX() - want_x > 2 ){
				setFacing( FACING_LEFT );
				moveX( getSpeed() );
				walk = true;
			}

			if ( getZ() < want_z ){
				moveZ( getSpeed() );
				walk = true;
			} else if ( getZ() > want_z ){
				moveZ( -getSpeed() );
				walk = true;
			}

			if ( walk ){
				animation_current = getMovement( "walk" );
			}

			if ( fabs(getX() - want_x) <= 2 &&
					fabs(getZ() - want_z) <= 2 ){
				want_x = -1;
				want_z = -1;
				animation_current = getMovement( "idle" );
			}
		}
	}
}
	
int BuddyPlayer::spawnTime(){
	spawn_time -= 1;
	return spawn_time;
}

void BuddyPlayer::deathReset(){
	setY( 200 );
	spawn_time = SPAWN_TIME;
	setMoving( true );
	setStatus( Status_Falling );
	setHealth( getMaxHealth() );
	setDeath( 0 );
	animation_current = getMovement( "idle" );
}
	
void BuddyPlayer::takeDamage( World * world, ObjectAttack * obj, int x ){
	Character::takeDamage( world, obj, x );
}
	
void BuddyPlayer::hurt( int x ){
	if ( ! isInvincible() ){
		Character::hurt( x );
	}
}
