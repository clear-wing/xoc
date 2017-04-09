/*@
Copyright (c) 2013-2014, Su Zhenyu steven.known@gmail.com

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Su Zhenyu nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

author: Su Zhenyu
@*/
#include "cominc.h"
#include "comopt.h"

namespace xoc {

//#define TRAVERSE_IN_DOM_TREE_ORDER


//
//START MDSSAMgr
//
size_t MDSSAMgr::count_mem()
{
    size_t count = 0;
    count += m_map_md2stack.count_mem();
    count += m_map_bb2philist.count_mem();
    count += m_max_version.count_mem();
    count += m_usedef_mgr.count_mem();
    count += sizeof(MDSSAMgr);
    return count;
}


//is_reinit: this function is invoked in reinit().
void MDSSAMgr::destroy(bool is_reinit)
{
    if (m_usedef_mgr.m_mdssainfo_pool == NULL) { return; }
    
    //Caution: if you do not finish out-of-SSA prior to destory().
    //The reference to IR's SSA info will lead to undefined behaviors.
    //ASSERT(!m_is_ssa_constructed,
    //   ("Still in ssa mode, you should do out of "
    //    "SSA before destroy"));

    cleanMD2Stack();
    if (is_reinit) {
        m_max_version.clean();
        m_usedef_mgr.reinit();
    }
    destroyMDSSAInfo();
    freePhiList();
}


void MDSSAMgr::freePhiList()
{
    for (IRBB * bb = m_ru->getBBList()->get_head(); 
         bb != NULL; bb = m_ru->getBBList()->get_next()) {
        MDPhiList * philist = m_usedef_mgr.genBBPhiList(BB_id(bb));
        if (philist == NULL) { continue; }
        
        for (SC<MDPhi*> * sct = philist->get_head();
             sct != philist->end(); sct = philist->get_next(sct)) {
            MDPhi * phi = sct->val();
            ASSERT0(phi && phi->is_phi());
            m_ru->freeIRTreeList(phi->getOpndList());
        }
    }
}


//Clean version stack.
void MDSSAMgr::cleanMD2Stack()
{
    for (INT i = 0; i <= m_map_md2stack.get_last_idx(); i++) {
        Stack<VMD*> * s = m_map_md2stack.get(i);
        if (s != NULL) { delete s; }
    }
    m_map_md2stack.clean();
}


//Dump ssa du stmt graph.
void MDSSAMgr::dumpSSAGraph(CHAR *)
{
    //MDSSAGraph sa(m_ru, this);
    //sa.dump(name, true);
}


CHAR * MDSSAMgr::dumpVMD(IN VMD * v, OUT CHAR * buf)
{
    sprintf(buf, "MD%dV%d", v->mdid(), v->version());
    return buf;
}


//This function dumps VMD structure and SSA DU info.
//have_renamed: set true if PRs have been renamed in construction.
void MDSSAMgr::dumpAllVMD()
{
    //if (g_tfile == NULL) { return; }
    //fprintf(g_tfile, "\n==---- DUMP MDSSAMgr:VMD ----==\n");
    //
    //for (INT i = 1; i <= getVMDVec()->get_last_idx(); i++) {
    //    VMD * v = getVMDVec()->get(i);
    //    ASSERT0(v);
    //    fprintf(g_tfile, "\nVMD%d:MD%dV%d: ", 
    //        VOPND_id(v), VOPND_mdid(v), VOPND_ver(v));
    //    IR * def = VOPND_def(v);
    //    if (VOPND_ver(v) != 0) {
    //        //After renaming, MD must have defstmt if its version is nonzero.
    //        ASSERT0(def);
    //    }
    //    if (def != NULL) {
    //        ASSERT0(def->is_stmt() && !def->isWritePR());
    //        fprintf(g_tfile, "DEF:%s,id%d", IRNAME(def), def->id());
    //    } else {
    //        fprintf(g_tfile, "DEF:---");
    //    }
    //
    //    fprintf(g_tfile, "\tUSE:");
    //
    //    MDSSAUseIter it = NULL;
    //    INT nexti = 0;
    //    for (INT j = VMD_uses(v).get_first(&it); it != NULL; j = nexti) {
    //        nexti = VMD_uses(v).get_next(j, &it);
    //        IR * use = m_ru->getIR(j);
    //        ASSERT0(!use->isReadPR());
    //
    //        fprintf(g_tfile, "(%s,id%d)", IRNAME(use), IR_id(use));
    //
    //        if (nexti >= 0) {
    //            fprintf(g_tfile, ",");
    //        }
    //    }
    //}
    fflush(g_tfile);
}


void MDSSAMgr::dump()
{
    if (g_tfile == NULL) { return; }
    fprintf(g_tfile, "\n\n==---- DUMP MDSSAMgr ----==\n");

    BBList * bbl = m_ru->getBBList();
    List<IR const*> lst;
    List<IR const*> opnd_lst;
    
    g_indent = 4;
    for (IRBB * bb = bbl->get_head(); bb != NULL; bb = bbl->get_next()) {
        fprintf(g_tfile, "\n--- BB%d ---", BB_id(bb));
        
        MDPhiList * philist = m_usedef_mgr.genBBPhiList(BB_id(bb));
        ASSERT0(philist);
        for (SC<MDPhi*> * sct = philist->get_head();
             sct != philist->end(); sct = philist->get_next(sct)) {
            MDPhi * phi = sct->val();
            ASSERT0(phi && phi->is_phi());
            note("\n");
            phi->dump(m_ru, &m_usedef_mgr);
        }

        for (IR * ir = BB_first_ir(bb);
             ir != NULL; ir = BB_next_ir(bb)) {
            dump_ir(ir, m_tm);

            bool parting_line = false;
            //Result
            if (ir->isMemoryRef()) {
                MDSSAInfo * mdssainfo = m_usedef_mgr.readMDSSAInfo(ir);
                ASSERT0(mdssainfo);
                SEGIter * iter = NULL;
                if (!parting_line) {
                    note("\n----------------");
                    parting_line = true;
                }
                dump_ir(ir, m_tm, NULL, false);
                
                for (INT i = mdssainfo->getVOpndSet()->get_first(&iter); 
                     i >= 0; i = mdssainfo->getVOpndSet()->get_next(i, &iter)) {
                    note("\n--DEF:");
                    VMD * vopnd = (VMD*)m_usedef_mgr.getVOpnd(i);
                    ASSERT0(vopnd && vopnd->is_md());
                    if (vopnd->getDef() != NULL) {
                        ASSERT0(vopnd->getDef()->getOcc() == ir);
                    }                    
                    vopnd->dump(m_ru);
                }                
            }

            //Operand
            lst.clean();
            opnd_lst.clean();
            for (IR const* opnd = iterInitC(ir, lst);
                 opnd != NULL; opnd = iterNextC(lst)) {
                if (!opnd->isMemoryRefNotOperatePR() || opnd->is_stmt()) {
                    continue;
                }

                MDSSAInfo * mdssainfo = m_usedef_mgr.readMDSSAInfo(opnd);
                ASSERT0(mdssainfo);
                SEGIter * iter = NULL;
                if (!parting_line) {
                    note("\n----------------");
                    parting_line = true;
                }
                dump_ir(opnd, m_tm, NULL, false);
                note("\n--USE:");
                bool first = true;
                for (INT i = mdssainfo->getVOpndSet()->get_first(&iter); 
                     i >= 0; i = mdssainfo->getVOpndSet()->get_next(i, &iter)) {
                    VMD * vopnd = (VMD*)m_usedef_mgr.getVOpnd(i);
                    ASSERT0(vopnd && vopnd->is_md());
                    if (first) {
                        first = false;
                    } else {
                        fprintf(g_tfile, ",");
                    }
                    fprintf(g_tfile, "MD%dV%d", vopnd->mdid(), vopnd->version());
                }
            }
        }
    }

    fflush(g_tfile);
}


//Generate VMD for stmt and exp which reference memory.
void MDSSAMgr::initVMD(IN IR * ir, OUT DefSBitSet & maydef_md)
{
    m_iter.clean();
    for (IR * t = iterInit(ir, m_iter); t != NULL; t = iterNext(m_iter)) {
        if (!t->isMemoryRefNotOperatePR() && !t->isCallStmt()) {
            continue;
        }

        MDSSAInfo * mdssainfo = m_usedef_mgr.genMDSSAInfo(t);

        MD const* ref = t->getRefMD();
        if (ref != NULL) {
            VMD const* vmd = m_usedef_mgr.allocVMD(MD_id(ref), 0);
            ASSERT0(m_sbs_mgr);
            mdssainfo->getVOpndSet()->append(vmd, *m_sbs_mgr);
            if (t->is_stmt()) {
                maydef_md.bunion(MD_id(ref));
            }
        }

        MDSet const* refset = t->getRefMDSet();
        if (refset != NULL) {
            SEGIter * iter;
            for (INT i = refset->get_first(&iter); 
                 i >= 0; i = refset->get_next((UINT)i, &iter)) {
                MD * md = m_md_sys->getMD(i);
                ASSERT0(md);
                if (!md->is_effect()) { continue; }
                VMD const* vmd2 = m_usedef_mgr.allocVMD(MD_id(md), 0);
                ASSERT0(m_sbs_mgr);
                mdssainfo->getVOpndSet()->append(vmd2, *m_sbs_mgr);
                if (t->is_stmt()) {
                    maydef_md.bunion(MD_id(md));
                }
            }
        }            
    }
}


//'maydef_md': record MDs that defined in 'bb'.
void MDSSAMgr::collectDefinedMD(IN IRBB * bb, OUT DefSBitSet & maydef_md)
{
    for (IR * ir = BB_first_ir(bb); ir != NULL; ir = BB_next_ir(bb)) {        
        initVMD(ir, maydef_md);
    }
}


void MDSSAMgr::insertPhi(UINT mdid, IN IRBB * bb)
{
    UINT num_opnd = m_cfg->get_in_degree(m_cfg->get_vertex(BB_id(bb)));

    //Here each operand and result of phi set to same type.
    //They will be revised to correct type during renaming.    
    MDPhi * phi = m_usedef_mgr.allocMDPhi(mdid, num_opnd);
    MDDEF_bb(phi) = bb;
    MDDEF_result(phi) = m_usedef_mgr.allocVMD(mdid, 0);
    ASSERT0(phi);

    m_usedef_mgr.genBBPhiList(bb->id())->append_head(phi);
}


//Insert phi for VMD.
//defbbs: record BBs which defined the VMD identified by 'mdid'.
void MDSSAMgr::placePhiForMD(
        UINT mdid,
        IN List<IRBB*> * defbbs,
        DfMgr const& dfm,
        BitSet & visited,
        List<IRBB*> & wl,
        Vector<DefSBitSet*> & defmds_vec)
{
    visited.clean();
    wl.clean();
    for (IRBB * defbb = defbbs->get_head();
         defbb != NULL; defbb = defbbs->get_next()) {
        wl.append_tail(defbb);
        visited.bunion(BB_id(defbb));
    }

    while (wl.get_elem_count() != 0) {
        IRBB * bb = wl.remove_head();

        //Each basic block in dfcs is in dominance frontier of 'bb'.
        BitSet const* dfcs = dfm.readDFControlSet(BB_id(bb));
        if (dfcs == NULL) { continue; }

        for (INT i = dfcs->get_first(); i >= 0; i = dfcs->get_next(i)) {
            if (visited.is_contain(i)) {
                //Already insert phi for 'mdid' into BB i.
                //TODO:ensure the phi for same PR does NOT be
                //inserted multiple times.
                continue;
            }

            visited.bunion(i);

            IRBB * ibb = m_cfg->getBB(i);
            ASSERT0(ibb);

            //Redundant phi will be removed during refinePhi().
            insertPhi(mdid, ibb);

            ASSERT0(defmds_vec.get(i));
            defmds_vec.get(i)->bunion(mdid);

            wl.append_tail(ibb);
        }
    }
}


//Return true if phi is redundant, otherwise return false.
//If all opnds have same defintion or defined by current
//phi, the phi is redundant.
//common_def: record the common_def if the definition of all opnd is the same.
//TODO: p=phi(m,p), the only use of p is phi. the phi is redundant.
bool MDSSAMgr::isRedundantPHI(MDPhi const* phi, OUT VMD ** common_def) const
{
    ASSERT0(phi && common_def);
    
    if (xcom::cnt_list(phi->getOpndList()) == 0) { return true; }
    
    #define DUMMY_DEF_ADDR    0x1234
    VMD * def = NULL;
    bool same_def = true; //indicate all DEF of operands are the same stmt.
    
    for (IR const* opnd = phi->getOpndList();
         opnd != NULL; opnd = opnd->get_next()) {
        VMD * v = phi->getOpndVMD(opnd, &m_usedef_mgr);
        if (v == NULL) { continue; }
        ASSERT0(v->is_md());

        if (v->getDef() != NULL) {
            if (def == NULL) {
                def = v->getDef()->getResult();
            } else if (def != v->getDef()->getResult() && def != phi->getResult()) {
                same_def = false;
                break;
            }
        } else {
            //DEF is dummy value to inidcate the region live-in MD.
            same_def = false;
            break;
        }
    }
    
    ASSERT0(common_def);
    *common_def = def;
    return same_def;
}


//Place phi and assign the v0 for each PR.
//'effect_prs': record the MD which need to versioning.
void MDSSAMgr::placePhi(DfMgr const& dfm,
                        OUT DefSBitSet & effect_md,
                        DefMiscBitSetMgr & bs_mgr,
                        Vector<DefSBitSet*> & defined_md_vec,
                        List<IRBB*> & wl)
{
    START_TIMERS("SSA: Place phi", t2);
    
    //Record BBs which modified each MD.
    BBList * bblst = m_ru->getBBList();
    Vector<List<IRBB*>*> md2defbb(bblst->get_elem_count()); //for local used.

    for (IRBB * bb = bblst->get_head(); bb != NULL; bb = bblst->get_next()) {
        DefSBitSet * bs = bs_mgr.allocSBitSet();
        defined_md_vec.set(BB_id(bb), bs);
        collectDefinedMD(bb, *bs);

        //Record all modified MDs which will be versioned later.
        effect_md.bunion(*bs);

        //Record which BB defined these effect mds.
        SEGIter * cur = NULL;
        for (INT i = bs->get_first(&cur); i >= 0; i = bs->get_next(i, &cur)) {
            List<IRBB*> * bbs = md2defbb.get(i);
            if (bbs == NULL) {
                bbs = new List<IRBB*>();
                md2defbb.set(i, bbs);
            }
            bbs->append_tail(bb);
        }
    }

    //Place phi for lived MD.
    BitSet visited((bblst->get_elem_count()/8)+1);
    SEGIter * cur = NULL;
    for (INT i = effect_md.get_first(&cur);
         i >= 0; i = effect_md.get_next(i, &cur)) {
        placePhiForMD(i, md2defbb.get(i), dfm, visited, wl, defined_md_vec);
    }
    END_TIMERS(t2);

    //Free local used objects.
    for (INT i = 0; i <= md2defbb.get_last_idx(); i++) {
        List<IRBB*> * bbs = md2defbb.get(i);
        if (bbs == NULL) { continue; }
        delete bbs;
    }
}


void MDSSAMgr::renameUse(IR * ir)
{
    ASSERT0(ir && ir->is_exp());

    MDSSAInfo * mdssainfo = m_usedef_mgr.genMDSSAInfo(ir);
    ASSERT0(mdssainfo);

    SEGIter * iter;
    VOpndSet * set = mdssainfo->getVOpndSet();
    VOpndSet removed;
    VOpndSet added;
    INT next;
    for (INT i = set->get_first(&iter); i >= 0; i = next) {
        next = set->get_next(i, &iter);
        VMD * vopnd = (VMD*)m_usedef_mgr.getVOpnd(i);
        ASSERT0(vopnd && vopnd->is_md() && vopnd->id() == (UINT)i);

        //Get the top-version on stack.
        Stack<VMD*> * vs = mapMD2VMDStack(vopnd->mdid());
        ASSERT0(vs);
        VMD * topv = vs->get_top();
        if (topv == NULL) {
            //MD does not have top-version, it has no def, 
			//and may be parameter.
            continue;
        }

        //e.g: MD1 = MD2(VMD1)
        //    VMD1 will be renamed to VMD2, so VMD1 will not
        //    be there in current IR any more.        

        //Set newest version of VMD be the USE of current opnd.
        if (VMD_version(topv) == 0) {
            //Each opnd only meet once.
            ASSERT0(vopnd == (VOpnd*)topv);
            ASSERT0(!topv->getOccSet()->find(ir));
        } else if (vopnd != (VOpnd*)topv) {
            //vopnd may be ver0.
            //Current ir does not refer the old version VMD any more.
            ASSERT0(VMD_version(vopnd) == 0 || 
                ((VMD*)vopnd)->getOccSet()->find(ir));
            ASSERT0(VMD_version(vopnd) == 0 || ((VMD*)vopnd)->getDef());
            ASSERT0(!((VMD*)topv)->getOccSet()->find(ir));

            set->remove(vopnd, *m_sbs_mgr);
            added.append(topv, *m_sbs_mgr);
        }

        topv->getOccSet()->append(ir);
    }

    set->bunion(added, *m_sbs_mgr);
    added.clean(*m_sbs_mgr);
}


void MDSSAMgr::renameDef(IR * ir, IRBB * bb)
{
    ASSERT0(ir && ir->is_stmt());

    MDSSAInfo * mdssainfo = m_usedef_mgr.genMDSSAInfo(ir);
    ASSERT0(mdssainfo);

    SEGIter * iter;
    VOpndSet * set = mdssainfo->getVOpndSet();    
    VOpndSet added;
    INT next;
    for (INT i = set->get_first(&iter); i >= 0; i = next) {
        next = set->get_next(i, &iter);
        VMD * vopnd = (VMD*)m_usedef_mgr.getVOpnd(i);
        ASSERT0(vopnd && vopnd->is_md() && vopnd->id() == (UINT)i);
        ASSERT(vopnd->version() == 0, ("should be first meet"));

        UINT maxv = m_max_version.get(vopnd->mdid());
        VMD * newv = m_usedef_mgr.allocVMD(vopnd->mdid(), maxv + 1);
        m_max_version.set(vopnd->mdid(), maxv + 1);

        VMD * nearestv = mapMD2VMDStack(vopnd->mdid())->get_top();

        mapMD2VMDStack(vopnd->mdid())->push(newv);

        MDDef * mddef = m_usedef_mgr.allocMDDef();
        MDDEF_bb(mddef) = bb;
        MDDEF_result(mddef) = newv;
        MDDEF_is_phi(mddef) = false;
        if (nearestv != NULL) {
            MDDEF_prev(mddef) = nearestv->getDef();
        }
        MDDEF_occ(mddef) = ir;
        
        VMD_def(newv) = mddef;

        set->remove(vopnd, *m_sbs_mgr);
        added.append(newv, *m_sbs_mgr);
    }

    set->bunion(added, *m_sbs_mgr);
    added.clean(*m_sbs_mgr);
}


//Rename VMD from current version to the top-version on stack if it exist.
void MDSSAMgr::renamePhiResult(IN IRBB * bb)
{
    ASSERT0(bb);
    MDPhiList * philist = m_usedef_mgr.genBBPhiList(BB_id(bb));
    ASSERT0(philist);
    for (SC<MDPhi*> * sct = philist->get_head();
         sct != philist->end(); sct = philist->get_next(sct)) {
        MDPhi * phi = sct->val();
        ASSERT0(phi && phi->is_phi() && phi->getBB() == bb);

        //Rename phi result.
        VMD * vopnd = phi->getResult();
        ASSERT0(vopnd && vopnd->is_md());

        UINT maxv = m_max_version.get(vopnd->mdid());
        VMD * newv = m_usedef_mgr.allocVMD(vopnd->mdid(), maxv + 1);
        m_max_version.set(vopnd->mdid(), maxv + 1);

        mapMD2VMDStack(vopnd->mdid())->push(newv);

        MDDEF_result(phi) = newv;
        MDDEF_prev(phi) = NULL;
        
        VMD_def(newv) = phi;
    }
}


//Rename VMD from current version to the top-version on stack if it exist.
void MDSSAMgr::renameBB(IN IRBB * bb)
{
    renamePhiResult(bb);
    for (IR * ir = BB_first_ir(bb); ir != NULL; ir = BB_next_ir(bb)) {
        //Rename opnd, not include phi.
        //Walk through rhs expression IR tree to rename IR_PR's VMD.
        m_iter.clean();
        for (IR * opnd = iterInit(ir, m_iter);
             opnd != NULL; opnd = iterNext(m_iter)) {
            if (!opnd->isMemoryOpnd() || opnd->isReadPR()) {
                continue;
            }

            MD const* ref = opnd->getRefMD();
            if (ref != NULL) {
                renameUse(opnd);
            }

            MDSet const* refset = opnd->getRefMDSet();
            if (refset != NULL) {
                SEGIter * iter;
                for (INT i = refset->get_first(&iter); 
                     i >= 0; i = refset->get_next((UINT)i, &iter)) {
                    MD * md = m_md_sys->getMD(i);
                    ASSERT0(md);
                    if (!md->is_effect()) { continue; }
                    renameUse(opnd);
                }
            }
        }

        if (!ir->isMemoryRef() || ir->isWritePR()) { continue; }

        //Rename result.
        renameDef(ir, bb);
    }
}


Stack<VMD*> * MDSSAMgr::mapMD2VMDStack(UINT mdid)
{
    Stack<VMD*> * stack = m_map_md2stack.get(mdid);
    if (stack == NULL) {
        stack = new Stack<VMD*>();
        m_map_md2stack.set(mdid, stack);
    }
    return stack;
}


void MDSSAMgr::handleBBRename(
        IRBB * bb,
        DefSBitSet & defed_mds,
        IN OUT BB2VMD & bb2vmd)
{
    ASSERT0(bb2vmd.get(BB_id(bb)) == NULL);
    Vector<VMD*> * ve_vec = new Vector<VMD*>();
    bb2vmd.set(BB_id(bb), ve_vec);

    SEGIter * cur = NULL;
    for (INT mdid = defed_mds.get_first(&cur);
         mdid >= 0; mdid = defed_mds.get_next(mdid, &cur)) {
        VMD * ve = mapMD2VMDStack(mdid)->get_top();
        ASSERT0(ve);
        ve_vec->set(VMD_mdid(ve), ve);
    }   
         
    renameBB(bb);

    //Rename PHI opnd in successor BB.
    List<IRBB*> succs;
    m_cfg->get_succs(succs, bb);
    if (succs.get_elem_count() ==0) { return; }

    //Replace the jth opnd of PHI with 'topv' which in bb's successor.
    List<IRBB*> preds;
    for (IRBB * succ = succs.get_head();
         succ != NULL; succ = succs.get_next()) {
        //Compute which predecessor 'bb' is with respect to its successor.
        m_cfg->get_preds(preds, succ);
        UINT idx = 0; //the index of corresponding predecessor.
        IRBB * p;
        for (p = preds.get_head();
             p != NULL; p = preds.get_next(), idx++) {
            if (p == bb) {
                break;
            }
        }
        ASSERT0(p);

        //Replace opnd of PHI of 'succ' with top SSA version.
        MDPhiList * philist = m_usedef_mgr.genBBPhiList(BB_id(succ));
        ASSERT0(philist);
        for (SC<MDPhi*> * sct = philist->get_head();
             sct != philist->end(); sct = philist->get_next(sct)) {
            MDPhi * phi = sct->val();
            ASSERT0(phi && phi->is_phi());
            
            UINT j = 0;
            IR * opnd;
            for (opnd = phi->getOpndList();
                 opnd != NULL && j < idx; opnd = opnd->get_next(), j++) {}
            ASSERT0(j == idx && opnd && opnd->is_id());
            
            ASSERT0(opnd->getRefMD());
            VMD * topv = mapMD2VMDStack(opnd->getRefMD()->id())->get_top();
            ASSERT(topv, ("miss def-stmt to operand of phi"));

            MDSSAInfo * opnd_ssainfo = m_usedef_mgr.readMDSSAInfo(opnd);
            ASSERT0(opnd_ssainfo);
            
            opnd_ssainfo->getVOpndSet()->clean(*m_sbs_mgr);            
            opnd_ssainfo->getVOpndSet()->append(topv, *m_sbs_mgr);
            topv->getOccSet()->append(opnd);
        }
    }    
}


//defed_prs_vec: for each BB, indicate PRs which has been defined.
void MDSSAMgr::renameInDomTreeOrder(
        IRBB * root,
        Graph & domtree,
        Vector<DefSBitSet*> & defed_mds_vec)
{
    Stack<IRBB*> stk;
    UINT n = m_ru->getBBList()->get_elem_count();
    BitSet visited(n / BIT_PER_BYTE);
    BB2VMD bb2vmd(n);
    IRBB * v;
    stk.push(root);
    List<IR*> lst; //for tmp use.
    while ((v = stk.get_top()) != NULL) {
        if (!visited.is_contain(BB_id(v))) {
            visited.bunion(BB_id(v));
            DefSBitSet * defed_mds = defed_mds_vec.get(BB_id(v));
            ASSERT0(defed_mds);
            handleBBRename(v, *defed_mds, bb2vmd);
        }

        Vertex const* bbv = domtree.get_vertex(BB_id(v));
        bool all_visited = true;
        for (EdgeC const* c = VERTEX_out_list(bbv); c != NULL; c = EC_next(c)) {
            Vertex * dom_succ = EDGE_to(EC_edge(c));
            if (dom_succ == bbv) { continue; }
            if (!visited.is_contain(VERTEX_id(dom_succ))) {
                ASSERT0(m_cfg->getBB(VERTEX_id(dom_succ)));
                all_visited = false;
                stk.push(m_cfg->getBB(VERTEX_id(dom_succ)));
                break;
            }
        }

        if (all_visited) {
            stk.pop();

            //Do post-processing while all kids of BB has been processed.
            Vector<VMD*> * ve_vec = bb2vmd.get(BB_id(v));
            ASSERT0(ve_vec);
            DefSBitSet * defed_mds = defed_mds_vec.get(BB_id(v));
            ASSERT0(defed_mds);

            SEGIter * cur = NULL;
            for (INT i = defed_mds->get_first(&cur);
                 i >= 0; i = defed_mds->get_next(i, &cur)) {
                Stack<VMD*> * vs = mapMD2VMDStack(i);
                ASSERT0(vs->get_bottom());
                VMD * ve = ve_vec->get(VMD_mdid(vs->get_top()));
                while (vs->get_top() != ve) {
                    vs->pop();
                }
            }

            bb2vmd.set(BB_id(v), NULL);
            delete ve_vec;
        }
    }

    #ifdef _DEBUG_
    for (INT i = 0; i <= bb2vmd.get_last_idx(); i++) {
        ASSERT0(bb2vmd.get(i) == NULL);
    }
    #endif
}


//Rename variables.
void MDSSAMgr::rename(
        DefSBitSet & effect_mds,
        Vector<DefSBitSet*> & defed_mds_vec,
        Graph & domtree)
{
    START_TIMERS("SSA: Rename", t);
    BBList * bblst = m_ru->getBBList();
    if (bblst->get_elem_count() == 0) { return; }

    SEGIter * cur = NULL;
    for (INT mdid = effect_mds.get_first(&cur);
         mdid >= 0; mdid = effect_mds.get_next(mdid, &cur)) {
        VMD * v = m_usedef_mgr.allocVMD(mdid, 0);
        mapMD2VMDStack(mdid)->push(v);
    }

    ASSERT0(m_cfg->get_entry());
    renameInDomTreeOrder(m_cfg->get_entry(), domtree, defed_mds_vec);
    END_TIMERS(t);
}


void MDSSAMgr::destructBBSSAInfo(IRBB * bb)
{
    MDPhiList * philist = m_usedef_mgr.genBBPhiList(BB_id(bb));
    ASSERT0(philist);    
    for (SC<MDPhi*> * sct = philist->get_head();
         sct != philist->end(); sct = philist->get_next(sct)) {
        MDPhi * phi = sct->val();
        ASSERT0(phi && phi->is_phi());
        stripPhi(phi);
    }
}


void MDSSAMgr::destructionInDomTreeOrder(IRBB * root, Graph & domtree)
{
    Stack<IRBB*> stk;
    UINT n = m_ru->getBBList()->get_elem_count();
    BitSet visited(n / BIT_PER_BYTE);
    BB2VMD BB2VMD(n);
    IRBB * v;
    stk.push(root);
    while ((v = stk.get_top()) != NULL) {
        if (!visited.is_contain(BB_id(v))) {
            visited.bunion(BB_id(v));
            destructBBSSAInfo(v);
        }

        Vertex * bbv = domtree.get_vertex(BB_id(v));
        ASSERT(bbv, ("dom tree is invalid."));

        EdgeC * c = VERTEX_out_list(bbv);
        bool all_visited = true;
        while (c != NULL) {
            Vertex * dom_succ = EDGE_to(EC_edge(c));
            if (dom_succ == bbv) { continue; }
            if (!visited.is_contain(VERTEX_id(dom_succ))) {
                ASSERT0(m_cfg->getBB(VERTEX_id(dom_succ)));
                all_visited = false;
                stk.push(m_cfg->getBB(VERTEX_id(dom_succ)));
                break;
            }
            c = EC_next(c);
        }

        if (all_visited) {
            stk.pop();
            //Do post-processing while all kids of BB has been processed.
        }
    }
}


//This function perform SSA destruction via scanning BB in preorder
//traverse dominator tree.
//Return true if inserting copy at the head of fallthrough BB
//of current BB's predessor.
void MDSSAMgr::destruction(DomTree & domtree)
{
    START_TIMER_FMT_AFTER();

    BBList * bblst = m_ru->getBBList();
    if (bblst->get_elem_count() == 0) { return; }
    ASSERT0(m_cfg->get_entry());
    destructionInDomTreeOrder(m_cfg->get_entry(), domtree);
    destroyMDSSAInfo();
    m_is_ssa_constructed = false;    

    END_TIMER_FMT_AFTER(("SSA: destruction in dom tree order"));
}


//Return true if inserting copy at the head of fallthrough BB
//of current BB's predessor.
//Note that do not free phi at this function, it will be freed
//by user.
void MDSSAMgr::stripPhi(MDPhi * phi)
{
    //IRBB * bb = phi->getBB();
    //ASSERT0(bb);
    //
    //Vertex const* vex = m_cfg->get_vertex(BB_id(bb));
    //ASSERT0(vex);
    //
    ////Temprarory RP to hold the result of PHI.
    //IR * phicopy = m_ru->buildPR(phi->get_type());
    //phicopy->setRefMD(m_ru->genMDforPR(
    //    PR_no(phicopy), phicopy->get_type()), m_ru);
    //phicopy->cleanRefMDSet();
    //IR * opnd = PHI_opnd_list(phi);
    //
    ////opnd may be CONST, LDA, PR.
    ////ASSERT0(opnd->is_pr());
    //ASSERT0(PHI_ssainfo(phi));
    //
    //UINT pos = 0;
    //for (EdgeC * el = VERTEX_in_list(vex), * nextel = NULL;
    //     el != NULL; el = nextel, opnd = opnd->get_next(), pos++) {
    //    ASSERT0(find_position(VERTEX_in_list(vex), el) == pos);
    //
    //    nextel = EC_next(el);
    //    INT pred = VERTEX_id(EDGE_from(EC_edge(el)));
    //
    //    ASSERT0(opnd);
    //    IR * opndcopy = m_ru->dupIRTree(opnd);
    //    if (opndcopy->is_pr()) {
    //        opndcopy->copyRef(opnd, m_ru);
    //    }
    //
    //    //The copy will be inserted into related predecessor.
    //    IR * store_to_phicopy = m_ru->buildStorePR(
    //        PR_no(phicopy), phicopy->get_type(), opndcopy);
    //    store_to_phicopy->copyRef(phicopy, m_ru);
    //
    //    IRBB * p = m_cfg->getBB(pred);
    //    ASSERT0(p);
    //    IR * plast = BB_last_ir(p);
    //
    //    //In PHI node elimination to insert the copy in the predecessor block,
    //    //there is a check if last IR of BB is not a call then
    //    //place the copy there only.
    //    //However for call BB terminator, the copy will be placed at the start
    //    //of fallthrough BB.
    //    if (plast != NULL && plast->isCallStmt()) {
    //        IRBB * fallthrough = m_cfg->getFallThroughBB(p);
    //        if (!plast->is_terminate()) {
    //            //Fallthrough BB does not exist if the last ir is terminate.
    //            ASSERT(fallthrough, ("invalid control flow graph."));
    //            if (BB_irlist(fallthrough).get_head() == NULL ||
    //                !BB_irlist(fallthrough).get_head()->is_phi()) {
    //                BB_irlist(fallthrough).append_head(store_to_phicopy);
    //            } else {
    //                //Insert block to hold the copy.
    //                IRBB * newbb = m_ru->allocBB();
    //                m_ru->getBBList()->insert_after(newbb, p);
    //                m_cfg->add_bb(newbb);
    //                m_cfg->insertVertexBetween(
    //                    BB_id(p), BB_id(fallthrough), BB_id(newbb));
    //                BB_is_fallthrough(newbb) = true;
    //
    //                //Then append the copy.
    //                BB_irlist(newbb).append_head(store_to_phicopy);
    //            }
    //        }
    //    } else {
    //        BB_irlist(p).append_tail_ex(store_to_phicopy);
    //    }
    //
    //    //Remove the SSA DU chain between opnd and its DEF stmt.
    //    if (opnd->is_pr()) {
    //        ASSERT0(MD_ssainfo(opnd));
    //        MDSSA_uses(MD_ssainfo(opnd)).remove(opnd);
    //    }
    //}
    //
    //IR * substitue_phi = m_ru->buildStorePR(
    //    PHI_prno(phi), phi->get_type(), phicopy);
    //substitue_phi->copyRef(phi, m_ru);
    //
    //BB_irlist(bb).insert_before(substitue_phi, phict);
    //
    //PHI_ssainfo(phi) = NULL;
}


//This function verify def/use information of PHI stmt.
//If vpinfo is available, the function also check VOPND_mdid of phi operands.
//is_vpinfo_avail: set true if VMD information is available.
bool MDSSAMgr::verifyPhi(bool is_vpinfo_avail)
{
    UNUSED(is_vpinfo_avail);
    BBList * bblst = m_ru->getBBList();
    List<IRBB*> preds;
    for (IRBB * bb = bblst->get_head(); bb != NULL; bb = bblst->get_next()) {
        m_cfg->get_preds(preds, bb);                

        MDPhiList * philist = m_usedef_mgr.genBBPhiList(BB_id(bb));
        if (philist == NULL) { continue; }
        
        for (SC<MDPhi*> * sct = philist->get_head();
             sct != philist->end(); sct = philist->get_next(sct)) {
            MDPhi * phi = sct->val();
            ASSERT0(phi);

            //Check phi result.
            VMD * res = phi->getResult();
            ASSERT0(res->is_md());

            //Check the number of phi opnds.
            UINT num_opnd = 0;
            
            for (IR const* opnd = phi->getOpndList();
                 opnd != NULL; opnd = opnd->get_next()) {            
                if (!opnd->is_id()) { continue; }
            
                //Opnd may be PR, CONST or LDA.
                MD const* opnd_md = opnd->getRefMD();
                ASSERT(opnd_md && MD_id(opnd_md) == res->mdid(),
                    ("mdid of VMD is unmatched"));

                //Ver0 is input parameter, and it has no MDSSA_def.
                //ASSERT0(VOPND_ver(MD_ssainfo(opnd)) > 0);

                num_opnd++;
            }           

            ASSERT(num_opnd == preds.get_elem_count(),
                  ("The number of phi operand must same with "
                   "the number of BB predecessors."));
        }        
    }
    return true;
}


bool MDSSAMgr::verifyVMD()
{
    //Check version for each vp.
    BitSet defset;
    Vector<VOpnd*> * vec = m_usedef_mgr.getVOpndVec();
    for (INT i = 1; i <= vec->get_last_idx(); i++) {
        VMD * v = (VMD*)vec->get(i);
        ASSERT0(v);

        if (!v->is_md()) { continue; }

        MDDef * def = v->getDef();
        if (def == NULL) {
            //ver0 used to indicate the Region live-in PR. It may be a parameter.
            ASSERT(v->version() == 0, ("Nondef vp's version must be 0"));
        } else {
            ASSERT(v->version() != 0, ("version can not be 0"));
            
            ASSERT(!defset.is_contain(def->id()),
                    ("DEF for each md+version must be unique."));
            defset.bunion(def->id());
        }

        VMD * res = NULL;
        if (def != NULL) {
            res = def->getResult();
            ASSERT0(res);

            if (!def->is_phi()) {
                ASSERT0(def->getOcc() && def->getOcc()->is_stmt() && 
                    def->getOcc()->isMemoryRef());

                bool findref = false;
                if (def->getOcc()->getRefMD() != NULL && 
                    MD_id(def->getOcc()->getRefMD()) == res->mdid()) {
                    findref = true;
                }
                if (def->getOcc()->getRefMDSet() != NULL &&
                    def->getOcc()->getRefMDSet()->is_contain_pure(res->mdid())) {
                    findref = true;
                }
                ASSERT0(findref);
            }
        }

        if (res != NULL) {
            SEGIter * vit = NULL;
            for (INT i2 = res->getOccSet()->get_first(&vit);
                 i2 >= 0; i2 = res->getOccSet()->get_next(i2, &vit)) {
                IR * use = m_ru->getIR(i2);

                ASSERT0(use->isMemoryRef() || use->is_id());

                bool findref = false;
                if (use->getRefMD() != NULL && 
                    MD_id(use->getRefMD()) == res->mdid()) {
                    findref = true;
                }
                if (use->getRefMDSet() != NULL &&
                    use->getRefMDSet()->is_contain_pure(res->mdid())) {
                    findref = true;
                }
                ASSERT0(findref);
            }
        }
    }

    return true;
}


void MDSSAMgr::verifySSAInfo(IR * ir)
{
    ASSERT0(ir);
    MDSSAInfo * mdssainfo = m_usedef_mgr.genMDSSAInfo(ir);
    ASSERT0(mdssainfo);
    SEGIter * iter;
    VOpndSet * set = mdssainfo->getVOpndSet();
    for (INT i = set->get_first(&iter); i >= 0; i = set->get_next(i, &iter)) {
        VMD * vopnd = (VMD*)m_usedef_mgr.getVOpnd(i);
        ASSERT0(vopnd && vopnd->is_md());
        MDDef * def = vopnd->getDef();
        
        if (ir->is_stmt()) {
            ASSERT(vopnd->version() != 0, ("not yet perform renaming"));
            ASSERT(def && def->getOcc() == ir, ("IR stmt should have MDDef"));            
        }
                
        if (def != NULL) {
            ASSERT0(def->getResult());
            if (!def->is_phi()) {
                ASSERT0(def->getOcc() && def->getOcc()->is_stmt());
                bool findref = false;
                if (def->getOcc()->getRefMD() != NULL && 
                    MD_id(def->getOcc()->getRefMD()) == vopnd->mdid()) {
                    findref = true;
                }
                if (def->getOcc()->getRefMDSet() != NULL &&
                    def->getOcc()->getRefMDSet()->is_contain_pure(vopnd->mdid())) {
                    findref = true;
                }
                ASSERT0(findref);
            }        
        }

        //Check SSA uses.
        SEGIter * iter2;
        for (INT i = vopnd->getOccSet()->get_first(&iter2); 
            i >= 0; i = vopnd->getOccSet()->get_next(i, &iter2)) {
            IR const* occ = (IR*)m_ru->getIR(i);
            ASSERT0(occ);

            bool findref = false;
            if (occ->getRefMD() != NULL && 
                MD_id(occ->getRefMD()) == vopnd->mdid()) {
                findref = true;
            }
            if (occ->getRefMDSet() != NULL &&
                occ->getRefMDSet()->is_contain_pure(vopnd->mdid())) {
                findref = true;
            }
            ASSERT0(findref);
        }        
    }
}


//The verification check the DU info in SSA form.
//Current IR must be in SSA form.
bool MDSSAMgr::verify()
{
    //Check version for each vp.    
    BBList * bbl = m_ru->getBBList();
    C<IRBB*> * ct;
    for (bbl->get_head(&ct); ct != bbl->end(); ct = bbl->get_next(ct)) {
        IRBB * bb = ct->val();
        C<IR*> * ctir;
        for (BB_irlist(bb).get_head(&ctir);
             ctir != BB_irlist(bb).end();
             ctir = BB_irlist(bb).get_next(ctir)) {

            //Verify PHI list.
            MDPhiList * philist = m_usedef_mgr.genBBPhiList(BB_id(bb));
            ASSERT0(philist);
            for (SC<MDPhi*> * sct = philist->get_head();
                 sct != philist->end(); sct = philist->get_next(sct)) {
                MDPhi * phi = sct->val();
                ASSERT0(phi && phi->is_phi() && phi->getResult());
                
                for (IR const* opnd = phi->getOpndList();
                     opnd != NULL; opnd = opnd->get_next()) {
                    if (!opnd->is_id()) { continue; }
                    
                    MD const* opnd_md = opnd->getRefMD();                    
                    ASSERT(opnd_md && 
                        MD_id(opnd_md) == phi->getResult()->mdid(),
                        ("MD not matched"));
                }
            }
    
            IR * ir = ctir->val();
            m_iter.clean();
            for (IR * x = iterInit(ir, m_iter);
                 x != NULL; x = iterNext(m_iter)) {
                if (x->isMemoryRefNotOperatePR()) {
                    verifySSAInfo(x);
                }
            }
        }
    }
    return true;
}


//This function perform SSA destruction via scanning BB in sequential order.
void MDSSAMgr::destruction()
{
    BBList * bblst = m_ru->getBBList();
    if (bblst->get_elem_count() == 0) { return; }

    C<IRBB*> * bbct;
    for (bblst->get_head(&bbct);
         bbct != bblst->end(); bbct = bblst->get_next(bbct)) {
        IRBB * bb = bbct->val();
        ASSERT0(bb);
        destructBBSSAInfo(bb);
    }

    //Clean SSA info to avoid unnecessary abort or assert.
    destroyMDSSAInfo();
    m_is_ssa_constructed = false;
}


//Set SSAInfo of IR to be NULL to inform optimizer that IR is not in SSA form.
void MDSSAMgr::destroyMDSSAInfo()
{
    for (INT i = 0; i <= m_ru->getIRVec()->get_last_idx(); i++) {
        IR * ir = m_ru->getIR(i);
        if (ir == NULL || 
            ir->getAI() == NULL ||
            ir->getAI()->get(AI_MD_SSA) == NULL) { 
            continue; 
        }            
        ((MDSSAInfo*)ir->getAI()->get(AI_MD_SSA))->destroy(*m_sbs_mgr);
        IR_ai(ir)->clean(AI_MD_SSA);
    }    
}


//This function revise phi data type, and remove redundant phi.
//wl: work list for temporary used.
void MDSSAMgr::refinePhiForBB(List<IRBB*> & wl, IRBB * bb)
{
    ASSERT0(bb);
    MDPhiList * philist = m_usedef_mgr.genBBPhiList(BB_id(bb));
    ASSERT0(philist);
    SC<MDPhi*> * next;
    for (SC<MDPhi*> * sct = philist->get_head(); 
         sct != philist->end(); sct = next) {
        next = philist->get_next(sct);
        MDPhi * phi = sct->val();

        ASSERT0(phi);

        VMD * common_def = NULL;
        if (isRedundantPHI(phi, &common_def)) {
            ASSERT0(common_def);

            for (IR * opnd = phi->getOpndList();
                 opnd != NULL; opnd = opnd->get_next()) {
                VMD * vopnd = phi->getOpndVMD(opnd, &m_usedef_mgr);
                if (vopnd == NULL) { continue; }

                ASSERT0(vopnd->is_md());
                if (vopnd->getDef()) {
                    ASSERT0(vopnd->getDef()->getBB());
                    wl.append_tail(vopnd->getDef()->getBB());
                }

                vopnd->getOccSet()->remove(opnd);
            }

            if (common_def != phi->getResult()) {
                ASSERT0(common_def->is_md());
                
                SEGIter * iter;
                IRSet * useset = phi->getResult()->getOccSet();
                for (INT i = useset->get_first(&iter); 
                     i >= 0; i = useset->get_next(i, &iter)) {
                    IR * u = m_ru->getIR(i);
                    ASSERT0(u && m_usedef_mgr.readMDSSAInfo(u));

                    //Change DEF.
                    m_usedef_mgr.readMDSSAInfo(u)->
                        getVOpndSet()->remove(phi->getResult(), *m_sbs_mgr);
                    m_usedef_mgr.readMDSSAInfo(u)->
                        getVOpndSet()->append(common_def, *m_sbs_mgr);
                }

                common_def->getOccSet()->bunion(*phi->getResult()->getOccSet());
            }

            philist->remove(phi);
            continue;
        }
    }
}


//This function revise phi data type, and remove redundant phi.
//wl: work list for temporary used.
void MDSSAMgr::refinePhi(List<IRBB*> & wl)
{
    START_TIMERS("SSA: Refine phi", t);

    BBList * bblst = m_ru->getBBList();
    C<IRBB*> * ct;

    wl.clean();
    for (bblst->get_head(&ct); ct != bblst->end(); ct = bblst->get_next(ct)) {
        IRBB * bb = ct->val();
        ASSERT0(bb);
        wl.append_tail(bb);
    }

    IRBB * bb = NULL;
    while ((bb = wl.remove_head()) != NULL) {
        refinePhiForBB(wl, bb);
    }

    END_TIMERS(t);
}


void MDSSAMgr::construction(OptCtx & oc)
{
    reinit();
    m_ru->checkValidAndRecompute(&oc, PASS_DOM, PASS_UNDEF);

    //Extract dominate tree of CFG.
    START_TIMERS("SSA: Extract Dom Tree", t4);
    DomTree domtree;
    m_cfg->get_dom_tree(domtree);
    END_TIMERS(t4);

    construction(domtree);

    OC_is_du_chain_valid(oc) = false; //DU chain of PR is voilated.
    m_is_ssa_constructed = true;
}


//Note: Non-SSA DU Chains of read/write PR will be clean and
//unusable after SSA construction.
void MDSSAMgr::construction(DomTree & domtree)
{
    ASSERT0(m_ru);

    START_TIMERS("SSA: DF Manager", t1);
    DfMgr dfm;
    dfm.build((DGraph&)*m_cfg); //Build dominance frontier.
    m_cfg->dump_vcg();
    dfm.dump((DGraph&)*m_cfg);
    domtree.dump_vcg("zdomtree.vcg");
    END_TIMERS(t1);

    List<IRBB*> wl;
    DefMiscBitSetMgr bs_mgr;
    DefSBitSet effect_mds(bs_mgr.getSegMgr());
    Vector<DefSBitSet*> defed_mds_vec;

    placePhi(dfm, effect_mds, bs_mgr, defed_mds_vec, wl);
    
    rename(effect_mds, defed_mds_vec, domtree);

    ASSERT0(verifyPhi(true));

    refinePhi(wl);

    //Clean version stack after renaming.
    cleanMD2Stack();

    dump();

    ASSERT0(verify());
    ASSERT0(verifyIRandBB(m_ru->getBBList(), m_ru));
    ASSERT0(verifyPhi(false) && verifyVMD());

    m_is_ssa_constructed = true;
}
//END MDSSAMgr

} //namespace xoc