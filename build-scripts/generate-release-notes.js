// ./tools/scripts/generate-release-notes.js

const github = require('@actions/github');

/**
 * Generates the release notes for a github release.
 *
 * Arguments:
 * 1 - github_token
 * 2 - new version tag
 * 3 - commit SHA of new release
 * 4 - optional prerelease tag prefix to stay within one release lane
 * 5 - optional target branch name for the note header
 */
const token = process.argv[2];
const version = process.argv[3];
const comittish = process.argv[4];
const releaseTagPrefix = process.argv[5] || "";
const targetBranch = process.argv[6] || "";
const repo = process.env.REPOSITORY_NAME;
const owner = process.env.GITHUB_REPOSITORY_OWNER;

function format_request_error( error ) {
    // Octokit promises that all errors are https://github.com/octokit/request-error.js
    try {
        let out = `${error} (error ${error.name} code ${error.status})`;
        if( error.response?.data ) {
            // the response data is not the raw bytes we get from the server, but already
            // preprocessed and typified object. There's *probably* not much extra info
            // we can glean from this, but we shall try regardless
            out += "\n";
            const data = error.response.data;
            for( const key of Object.keys( data ) ) {
                out += `  ${key}: ${data[key]}\n`;
            }
        }
        return out;
    } catch( e ) {
        return `${error}`;
    }
}

async function main() {
    const client = github.getOctokit( token );
    let previousTag = "";
    let latestStableTag = "";
    let latestAnyTag = "";

    const releasesResponse = await client.request(
        'GET /repos/{owner}/{repo}/releases',
        {
            owner: owner,
            repo: repo,
            headers: {
                'X-GitHub-Api-Version': '2022-11-28',
            },
        }
    ).catch( ( e ) => {
        throw `${format_request_error( e )} ...when getting latest release`;
    } );

    if( releasesResponse.data ) {
        for( const responseData of releasesResponse.data ) {
            if( responseData.draft ) {
                continue;
            }

            if( !latestAnyTag ) {
                latestAnyTag = responseData.tag_name;
            }
            if( !responseData.prerelease && !latestStableTag ) {
                latestStableTag = responseData.tag_name;
            }

            if( responseData.prerelease && releaseTagPrefix && responseData.tag_name.startsWith( releaseTagPrefix ) ) {
                previousTag = responseData.tag_name;
                break;
            }
        }
    }

    if( !previousTag ) {
        previousTag = latestStableTag || latestAnyTag;
    }

    const requestBody = {
        owner: owner,
        repo: repo,
        tag_name: version,
        target_commitish: comittish,
        headers: {
            'X-GitHub-Api-Version': '2022-11-28',
        },
    };
    if( previousTag ) {
        requestBody.previous_tag_name = previousTag;
    }

    const response = await client.request(
        'POST /repos/{owner}/{repo}/releases/generate-notes',
        requestBody
    ).catch( ( e ) => {
        throw `${format_request_error( e )} ...when asking github to autogenerate release notes since tag '${previousTag || "<none>"}'`;
    } );

    const noteSections = response.data.body?.split( '\n\n' ) ?? [];
    const trimmedSections = [];
    const githubNotesMaxCharLength = 125000;
    const maxSectionLength = noteSections.length > 0 ? githubNotesMaxCharLength / noteSections.length : githubNotesMaxCharLength;

    for( let i = 0; i < noteSections.length; i++ ) {
        if( noteSections[i].length > githubNotesMaxCharLength ) {
            const lastLineIndex =
                noteSections[i].substring( 0, maxSectionLength ).split( '\n' ).length - 1;
            const trimmed =
                noteSections[i]
                .split( '\n' )
                .slice( 0, lastLineIndex - 1 )
                .join( '\n' ) +
                `\n... (+${noteSections[i].split( '\n' ).length - ( lastLineIndex + 1 )} others)`;
            trimmedSections.push( trimmed );
            continue;
        }

        trimmedSections.push( noteSections[i] );
    }

    const headerLines = [];
    if( targetBranch ) {
        headerLines.push( `Target branch: \`${targetBranch}\`` );
    }
    if( previousTag ) {
        headerLines.push( `Previous release reference: \`${previousTag}\`` );
    }

    if( headerLines.length > 0 ) {
        console.log( `${headerLines.join( '\n' )}\n\n${trimmedSections.join( '\n\n' )}` );
    } else {
        console.log( trimmedSections.join( '\n\n' ) );
    }
}

main().catch( ( e ) => {
    console.error( `Failed generating release notes with error: ${e}` );
    process.exit( 0 );
} );
