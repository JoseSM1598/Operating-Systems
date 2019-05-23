package edu.duke.raft;
import java.util.Random;
import java.util.Timer;

public class CandidateMode extends RaftMode {
    protected static Timer vote_timer;
    protected static Timer election_timer;
    protected static Random rand = new Random();
    private boolean leaderIsDead = false;


    public void setTimeout(){
        long random_timeout = (long) (rand.nextInt((ELECTION_TIMEOUT_MAX - ELECTION_TIMEOUT_MIN) + 1) + ELECTION_TIMEOUT_MIN);
        int override_timeout = mConfig.getTimeoutOverride();
        if (override_timeout != -1){
            random_timeout = mConfig.getTimeoutOverride();
        }
        election_timer = scheduleTimer(random_timeout, 0);
    }



  public void go () {
    synchronized (mLock) {

        // Increment term
        RaftResponses.clearVotes(mConfig.getCurrentTerm());
        int term = mConfig.getCurrentTerm();
        term++;
        mConfig.setCurrentTerm(term, mID);
        RaftResponses.setTerm(term);

        System.out.println ("S" +
			  mID + 
			  "." + 
			  term + 
			  ": switched to candidate mode.");

        int n = 1;
        int num_servers = mConfig.getNumServers();
        while (n <= num_servers) {
            if(n!=mID){
                remoteRequestVote(n, term, mID, mLog.getLastIndex(), mLog.getLastTerm());
            }
            n++;

        }

        // Set timers
        setTimeout();
        vote_timer = scheduleTimer(HEARTBEAT_INTERVAL, 1);

    }
  }

    private boolean candidateUpToDate(int lastIndex, int lastTerm){
        if(mLog.getLastTerm() != lastTerm) {
            return mLog.getLastTerm() < lastTerm;
        } else {
            return mLog.getLastIndex() <= lastIndex;
        }

    }

  // @param candidate’s term
  // @param candidate requesting vote
  // @param index of candidate’s last log entry
  // @param term of candidate’s last log entry
  // @return 0, if server votes for candidate; otherwise, server's
  // current term 
  public int requestVote (int candidateTerm,
			  int candidateID,
			  int lastLogIndex,
			  int lastLogTerm) {
    synchronized (mLock) {
        int term = mConfig.getCurrentTerm ();
        if (leaderIsDead){
            return term;
        }
        // Only vote for them if their term is HIGHER
        if (term >= candidateTerm){
            return term;

        }

        //Cancel timers because we could be switching to follower
        election_timer.cancel();
        vote_timer.cancel();
        RaftResponses.clearVotes(mConfig.getCurrentTerm());

        boolean isUpToDate = candidateUpToDate(lastLogIndex, lastLogTerm);
        if(isUpToDate){
            mConfig.setCurrentTerm(candidateTerm, candidateID);
            leaderIsDead = true;
            RaftServerImpl.setMode(new FollowerMode());
            return 0;
        }
        else{
            mConfig.setCurrentTerm(candidateTerm, 0);
            leaderIsDead = true;
            RaftServerImpl.setMode(new FollowerMode());
            return candidateTerm;
        }

    }
  }
  

  // @param leader’s term
  // @param current leader
  // @param index of log entry before entries to append
  // @param term of log entry before entries to append
  // @param entries to append (in order of 0 to append.length-1)
  // @param index of highest committed entry
  // @return 0, if server appended entries; otherwise, server's
  // current term
  public int appendEntries (int leaderTerm,
			    int leaderID,
			    int prevLogIndex,
			    int prevLogTerm,
			    Entry[] entries,
			    int leaderCommit) {
    synchronized (mLock) {

        int term = mConfig.getCurrentTerm ();
        if (leaderIsDead){
            return term;
        }

        //Will not append entries if leader is at a lower term
        if (leaderTerm < term){
            return term;
        }
        // else, will end up reverting to follower. However, other things need to get done first
        election_timer.cancel();
        vote_timer.cancel();

        int result = leaderTerm;
        RaftResponses.clearVotes(mConfig.getCurrentTerm());
        mConfig.setCurrentTerm(leaderTerm, 0);

        if(entries.length == 0){
            if (!(prevLogIndex == mLog.getLastIndex()) || !(prevLogIndex==mLog.getLastTerm())){
                result = leaderTerm;
            }
            else{
                result = 0;
            }
        }

        leaderIsDead = true;
        RaftServerImpl.setMode(new FollowerMode());
        return result;
    }
  }


  // @param id of the timer that timed out
  public void handleTimeout (int timerID) {
    synchronized (mLock) {
        if(leaderIsDead){ return; }
        if (timerID == 0){ // Election Timeout
            election_timer.cancel();
            vote_timer.cancel();
            // clear responses
            RaftResponses.clearVotes(mConfig.getCurrentTerm());
            int term = mConfig.getCurrentTerm() + 1;
            mConfig.setCurrentTerm(term, mID);
            RaftResponses.setTerm(term);

            int n = 1;
            int num_servers = mConfig.getNumServers();
            while (n <= num_servers){
                if (n != mID){
                    remoteRequestVote(n, mConfig.getCurrentTerm(), mID, mLog.getLastIndex(), mLog.getLastTerm());
                }
                n++;
            }

            setTimeout();
            vote_timer = scheduleTimer(HEARTBEAT_INTERVAL, 1);  // vote request (loop) timer


        }
        if (timerID == 1){ // Vote timeut
            int term = mConfig.getCurrentTerm();
            int[] votes = RaftResponses.getVotes(term);

            // Two variables to use so that I can keep track of votes
            int my_votes = 0;
            int maximum_term = Integer.MIN_VALUE;

            for (int i = 0; i < votes.length; i++){
                if (i == mID) continue;
                if (votes[i] > term){
                    maximum_term = Math.max(maximum_term, votes[i]);

                }
                else if ( votes[i] == 0){
                    my_votes++;
                }
            }

            // Did we get a majority of votes?
            boolean majority = (my_votes >= mConfig.getNumServers()/2);

            int curr_index = 0;
            if (maximum_term > term) { // FOLLOWER

                // Find the index of the server maximum_term belongs to
                for (int i = 1; i < votes.length; i++) {
                    if (votes[i] == maximum_term) {
                        curr_index = i;
                        break;
                    }
                }
                // TURN TO FOLLOWER
                vote_timer.cancel();
                election_timer.cancel();
                RaftResponses.clearVotes(mConfig.getCurrentTerm());
                leaderIsDead= true;
                mConfig.setCurrentTerm(maximum_term, curr_index);
                RaftServerImpl.setMode(new FollowerMode());
                return;
            }

            if (majority){
                election_timer.cancel();
                leaderIsDead = true;
                RaftServerImpl.setMode(new LeaderMode());
                return;
            }
            else{
                int n = 1;
                int num_servers = mConfig.getNumServers();
                while (n <= num_servers){
                    if (n != mID){
                        remoteRequestVote(n, mConfig.getCurrentTerm(), mID, mLog.getLastIndex(), mLog.getLastTerm());
                    }
                    n++;
                }
                vote_timer=scheduleTimer(HEARTBEAT_INTERVAL, 1);
            }

        }
    }
  }
}
