
#include "Globals.h"  // NOTE: MSVC stupidness requires this to be the same across all modules

#include "Piston.h"
#include "Redstone.h"
#include "ChunkDef.h"
#include "Pickup.h"
#include "Item.h"
#include "Root.h"
#include "ClientHandle.h"
#include "World.h"
#include "BlockID.h"
#include "Server.h"
#include "Blocks/BlockHandler.h"

extern bool g_BlockPistonBreakable[];

#define AddDir( x, y, z, dir, amount ) switch(dir) { case 0: (y)-=(amount); break; case 1: (y)+=(amount); break;\
													 case 2: (z)-=(amount); break; case 3: (z)+=(amount); break;\
													 case 4: (x)-=(amount); break; case 5: (x)+=(amount); break; }





cPiston::cPiston( cWorld* a_World )
	:m_World ( a_World )
{

}

unsigned short cPiston::FirstPassthroughBlock( int pistonX, int pistonY, int pistonZ, char pistonmeta ) {
	unsigned short ret;
	pistonmeta &= 7;
	if(pistonmeta >= 6) { // just in case, it shouldn't happen but if it would, it'd case inf loop
		printf("cPiston::FirstPassthroughBlock - piston has invalid meta data!\n");
		return 9001;
	}
	char currBlock;
	for( ret = 0; ret < 24; ret++ ) { // push up to 24 blocks
		AddDir( pistonX, pistonY, pistonZ, pistonmeta, 1 )
		currBlock = m_World->GetBlock( pistonX, pistonY, pistonZ );
		if(currBlock == E_BLOCK_BEDROCK || currBlock == E_BLOCK_OBSIDIAN || currBlock == E_BLOCK_PISTON_EXTENSION ) {return 9001;}
		if(g_BlockPistonBreakable[currBlock]) {return ret;}
	}
	return 9001;
}





void cPiston::ExtendPiston( int pistx, int pisty, int pistz )
{
	char pistonBlock = m_World->GetBlock( pistx, pisty, pistz );
	char pistonMeta = m_World->GetBlockMeta( pistx, pisty, pistz );
	char isSticky = (char)(pistonBlock == E_BLOCK_STICKY_PISTON) * 8;
	bool recalc = false;
	if ( (pistonMeta & 0x8) == 0x0 ) // only extend if piston is not already extended 
	{
		unsigned short dist = FirstPassthroughBlock(pistx, pisty, pistz, pistonMeta);
		if (dist > 9000) return; // too many blocks
	
		AddDir( pistx, pisty, pistz, pistonMeta & 7, dist+1 )
		BLOCKTYPE currBlock = m_World->GetBlock    (pistx, pisty, pistz);
		NIBBLETYPE currMeta = m_World->GetBlockMeta(pistx, pisty, pistz);
		if (currBlock != E_BLOCK_AIR)
		{
			cBlockHandler * Handler = BlockHandler(currBlock);
			if (Handler->DoesDropOnUnsuitable())
			{
				Handler->DropBlock(m_World, pistx, pisty, pistz);
			}
			recalc = true;
		}
		int oldx = pistx, oldy = pisty, oldz = pistz;
		char currBlockMeta;
		for (int i = dist + 1; i>0; i--)
		{
			AddDir(pistx, pisty, pistz, pistonMeta & 7, -1)
			currBlock = m_World->GetBlock(pistx, pisty, pistz);
			currBlockMeta = m_World->GetBlockMeta(pistx, pisty, pistz);
			m_World->SetBlock( oldx, oldy, oldz, currBlock, currBlockMeta);
			oldx = pistx;
			oldy = pisty;
			oldz = pistz;
		}
		m_World->BroadcastBlockAction(pistx, pisty, pistz, 0, pistonMeta, E_BLOCK_PISTON);
		m_World->FastSetBlock( pistx, pisty, pistz, pistonBlock, pistonMeta | 0x8 );

		int extx = pistx;
		int exty = pisty;
		int extz = pistz;
	
		AddDir(extx, exty, extz, pistonMeta & 7, 1)

		m_World->SetBlock(extx, exty, extz, E_BLOCK_PISTON_EXTENSION, isSticky + pistonMeta & 7);
		
		if (recalc)
		{
			cRedstone Redstone(m_World);
			Redstone.ChangeRedstone(extx,  exty,  extz,  false);  // recalculate redstone around current device
			Redstone.ChangeRedstone(pistx, pisty, pistz, false);  // recalculate redstone around current device
		}
	}
}





void cPiston::RetractPiston( int pistx, int pisty, int pistz )
{
	char pistonBlock = m_World->GetBlock( pistx, pisty, pistz );
	char pistonMeta = m_World->GetBlockMeta( pistx, pisty, pistz );
	if (pistonMeta <= 6)  // only retract if piston is not already retracted
	{
		return;
	}
	m_World->BroadcastBlockAction(pistx, pisty, pistz, 1, pistonMeta & ~(8), E_BLOCK_PISTON);
	m_World->FastSetBlock(pistx, pisty, pistz, pistonBlock, pistonMeta & ~(8));
	
	AddDir(pistx, pisty, pistz, pistonMeta & 7, 1)
	if (m_World->GetBlock(pistx, pisty, pistz) == E_BLOCK_PISTON_EXTENSION)
	{
		if (pistonBlock == E_BLOCK_STICKY_PISTON)
		{
			int tempx = pistx, tempy = pisty, tempz = pistz;
			AddDir( tempx, tempy, tempz, pistonMeta & 7, 1 )
			char tempblock = m_World->GetBlock( tempx, tempy, tempz );
			if (
				(tempblock == E_BLOCK_OBSIDIAN) || 
				(tempblock == E_BLOCK_BEDROCK) || 
				(tempblock == E_BLOCK_PISTON_EXTENSION)
			)
			{
				// These cannot be moved by the sticky piston, bail out
				return;
			}
			m_World->SetBlock( pistx, pisty, pistz, tempblock, m_World->GetBlockMeta( tempx, tempy, tempz ) );
			m_World->SetBlock( tempx, tempy, tempz, E_BLOCK_AIR, 0 );
		}
		else
		{
			m_World->SetBlock( pistx, pisty, pistz, E_BLOCK_AIR, 0 );
		}
	}
}



