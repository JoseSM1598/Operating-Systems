package edu.duke.raft;

import java.util.Timer;
import java.util.Random;

public class LeaderMode extends RaftMode {
    private Timer timer;
    int[] follower_log;
    private boolean crash = false;


    private void setTimeout() {
        timer = scheduleTimer(HEARTBEAT_INTERVAL, 0);
    }


    public void go() {
        synchronized (mLock) {
            int term = mConfig.getCurrentTerm();
            System.out.println("S" + mID + "." + term + ": switched to leader mode.");


            RaftResponses.clearAppendResponses(term);

            int num_servers = mConfig.getNumServers();
            follower_log = new int[num_servers + 1];
            int n = 0;
            while (n <= num_servers) {
                follower_log[n] = mLog.getLastIndex();
                n++;
            }

            n = 1;
            // Now, send empty heartbeat messages
            while (n < num_servers) {
                if (n != mID) {
                    remoteAppendEntries(n, term, mID, mLog.getLastIndex(), mLog.getLastTerm(), new Entry[0], mCommitIndex);
                }
                n++;
            }
            setTimeout();
        }
    }

    private void resetTimer() {
        timer.cancel();
        setTimeout();
    }

    private boolean candidateUpToDate(int lastIndex, int lastTerm) {
        if (mLog.getLastTerm() != lastTerm) {
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
    public int requestVote(int candidateTerm, int candidateID, int lastLogIndex, int lastLogTerm) {
        synchronized (mLock) {
            int term = mConfig.getCurrentTerm();
            if (crash) {
                return term;
            }
            if (term >= candidateTerm) {
                return term; // candidate must have a STRICTLY higher term
            }

            // Else, we will be reverting to a follower!
            timer.cancel();
            int vote = candidateTerm;

            boolean upToDate = candidateUpToDate(lastLogIndex, lastLogTerm);
            if (upToDate) {
                mConfig.setCurrentTerm(vote, candidateID);
                RaftServerImpl.setMode(new FollowerMode());
                return 0;
            } else { // not up to date
                mConfig.setCurrentTerm(candidateTerm, 0);
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
    public int appendEntries(int leaderTerm, int leaderID, int prevLogIndex, int prevLogTerm, Entry[] entries, int leaderCommit) {
        synchronized (mLock) {

            int term = mConfig.getCurrentTerm();
            if (crash) {
                return term;
            }
            if (term >= leaderTerm) {
                return term;
            }
            // else, we will end up reverting to a follower mode

            timer.cancel(); //go ahead and cancel this
            mConfig.setCurrentTerm(leaderTerm, 0);
            int result = leaderTerm;
            if(entries.length == 0){
                if (!(prevLogIndex == mLog.getLastIndex()) || !(prevLogIndex==mLog.getLastTerm())){
                    result = leaderTerm;
                }
                else{
                    result = 0;
                }
            }

            if(mLog.insert(entries, prevLogIndex, prevLogTerm) != -1) {
                result = 0;
            }
            crash = true;
            RaftServerImpl.setMode(new FollowerMode());
            return result;

        }
    }

    // @param id of the timer that timed out
    public void handleTimeout(int timerID) {
        synchronized (mLock) {
            if (crash) return;
            // HEARTBEAT TIMER TIMED OUT, HOW DO WE HANDLE THIS?
            timer.cancel();

            // Obtain all of the followers' responses
            int term = mConfig.getCurrentTerm();
            int[] follower_responses = RaftResponses.getAppendResponses(term);

            // TODO: Maybe do a check to see if follower_response is null
            assert follower_responses != null;
            for (int h = 1; h < follower_responses.length; h++) {
                if (h != mID) {
                    boolean follower_0 = follower_responses[h] == 0;
                    if (follower_0) {
                        follower_log[h] = mLog.getLastIndex();
                    } else if (follower_responses[h] <= term && !(follower_0)) {
                        if (!(follower_log[h] == -1)) {
                            follower_log[h]--;
                        }
                    } else if ((follower_responses[h] > mConfig.getCurrentTerm()) && !(follower_0)) {
                        RaftResponses.clearAppendResponses(term);
                        mConfig.setCurrentTerm(follower_responses[h], 0);
                        crash = true;

                        RaftServerImpl.setMode(new FollowerMode());
                        return;
                    }
                }
            }


            // SEND OUT A HEARTBEAT NOW THAT THE LOG IS FIXED
            RaftResponses.clearAppendResponses(term);

            int num_servers = mConfig.getNumServers();
            int n = 1;
            while (n <= num_servers) {
                if (n != mID) {
                    // Get the differences between the last index, and each of the serves index
                    int correction = -1 * follower_log[n] + mLog.getLastIndex();
                    Entry[] entry = new Entry[correction];
                    int i = 0;
                    while (i < correction) {
                        entry[i] = mLog.getEntry(follower_log[n] + 1 + i);
                        i++;
                    }
                    if (mLog.getEntry(follower_log[n]) == null) {
                        remoteAppendEntries(n, term, mID, follower_log[n], term, entry, mCommitIndex);
                    } else {
                        remoteAppendEntries(n, term, mID, follower_log[n], mLog.getEntry(follower_log[n]).term, entry, mCommitIndex);
                    }
                }
                n++;
            }


            setTimeout();


        }
    }
}
